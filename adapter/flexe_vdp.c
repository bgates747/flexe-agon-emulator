/* Flexe ESP32 backend for Fab's existing native VDP ABI.
 * Copyright 2026 Brandon Gates; reusable emulator integration, MIT.
 * UART2, VGA and audio come from executed firmware, never native VDU substitutes.
 */
#define _POSIX_C_SOURCE 200809L
#include "flexe_session.h"
#include "peripherals.h"
#include "rom_stubs.h"
#include "target.h"
#include "ulp.h"
#include "ps2.h"
#include "flexe_log.h"
#include "clock.h"
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

volatile int emu_app_running = 1;
#define QUEUE_CAPACITY (1024u * 1024u)
#define AUDIO_CAPACITY 65536u
#define RASTER_COLUMNS 1408u
#define RASTER_ROWS 1200u
#define SCREEN_COLUMNS 1024u
#define SCREEN_ROWS 768u
#define BATCH 10000

typedef struct {
    uint8_t data[QUEUE_CAPACITY];
    size_t head, size;
} byte_queue_t;
static pthread_mutex_t io_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t video_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t audio_lock = PTHREAD_MUTEX_INITIALIZER;
static byte_queue_t host_to_vdp, vdp_to_host, keyboard;
static atomic_bool stopping, ready, failed, debug_logging;
static atomic_uint_fast64_t frame_count, cycle_count, retired_count;
static atomic_uint_fast64_t rx_count, tx_count, delivered_count, key_count;
static atomic_uint uart_pending;
static flexe_session_t *session;
static elf_symbols_t *diagnostic_symbols;
static FILE *host_trace, *vdp_trace, *delivered_trace, *debug_trace;
static const char *artifacts;
static unsigned requested_mode = UINT32_MAX;
static uint8_t raster[RASTER_ROWS][RASTER_COLUMNS];
static uint8_t rgb[SCREEN_COLUMNS * SCREEN_ROWS * 3u];
static unsigned scan_x, scan_y, scan_columns, scan_rows;
static uint8_t previous_pixel;
static bool scan_started, console_sent;
static unsigned screen_width = 640, screen_height = 480;
static unsigned crop_left = 144, crop_top = 34, scan_repeat = 1;
static unsigned expected_columns = 800, expected_rows = 525;
static unsigned output_width = 640, output_height = 480;
static float output_rate = 60.0f;
static uint32_t pixel_rate;
static uint8_t audio_ring[AUDIO_CAPACITY];
static size_t audio_head, audio_size;
static unsigned packet_state, packet_code, packet_length, packet_used;
static uint8_t packet[256];
static agon_ulp_t ulp;
static agon_ps2_t ps2;
static struct {uint16_t code;uint8_t down;} key_events[4096];
static unsigned key_head,key_size;
static uint8_t *iram_shadow;
static uint32_t iram_start;
static size_t iram_length;
static atomic_uint_fast64_t instruction_syncs, code_invalidations;

static uint64_t monotonic_ns(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
}
static void delay_ns(uint64_t ns) {
    struct timespec t = {(time_t)(ns / 1000000000ull), (long)(ns % 1000000000ull)};
    while (nanosleep(&t, &t) && errno == EINTR) {}
}
static void fail(const char *message) {
    fprintf(stderr, "[flexe-agon-emulator] %s\n", message);
    atomic_store(&failed, true);
    atomic_store(&stopping, true);
}
static bool queue_put(byte_queue_t *q, uint8_t value) {
    if (q->size == QUEUE_CAPACITY) { fail("UART queue exhausted"); return false; }
    q->data[(q->head + q->size++) % QUEUE_CAPACITY] = value;
    return true;
}
static bool queue_get(byte_queue_t *q, uint8_t *value) {
    if (!q->size) return false;
    *value = q->data[q->head];
    q->head = (q->head + 1u) % QUEUE_CAPACITY;
    q->size--;
    return true;
}
static FILE *trace_open(const char *name) {
    if (!artifacts || !*artifacts) return NULL;
    char path[4096];
    if (snprintf(path, sizeof(path), "%s/%s", artifacts, name) >= (int)sizeof(path)) {
        fail("artifact path too long"); return NULL;
    }
    FILE *f = fopen(path, "wb");
    if (!f) fail("cannot open artifact output");
    return f;
}
/* Timings are the modelines used by Agon VDP's standard modes. Decode the
 * active raster from actual H/V sync; metadata selects its visible crop. */
static void select_geometry(unsigned width, unsigned height) {
    struct timing {unsigned w,h,ht,hs,vt,vs,repeat;};
    static const struct timing modes[] = {
        {640,480,800,656,525,490,1}, {640,240,800,656,262,245,2},
        {320,240,400,328,262,245,2}, {320,200,400,328,224,206,2},
        {512,384,672,524,403,385,2}, {640,512,844,664,533,513,2},
        {640,256,844,664,267,257,4}, {800,600,1056,840,628,601,1},
        {1024,768,1344,1048,806,771,1}
    };
    for (unsigned i=0; i<sizeof(modes)/sizeof(modes[0]); ++i) {
        const struct timing *t = &modes[i];
        if (width == t->w && height == t->h) {
            screen_width=width; screen_height=height;
            crop_left=t->ht-t->hs; crop_top=(t->vt-t->vs)*t->repeat-1u;
            scan_repeat=t->repeat; expected_columns=t->ht;
            expected_rows=t->vt*t->repeat;
            if(atomic_load(&debug_logging)) fprintf(stderr,"[flexe-agon-emulator] firmware mode %ux%u; scanout %ux%u crop %u/%u repeat %u\n",
                    width,height,expected_columns,expected_rows,crop_left,crop_top,scan_repeat);
            return;
        }
    }
    fprintf(stderr,"[flexe-agon-emulator] unsupported visible geometry %ux%u; retaining prior crop\n",width,height);
}
static void parse_packet(uint8_t byte) {
    if (packet_state == 0) {
        if (byte < 0x80u) return;
        packet_code=byte; packet_state=1;
    } else if (packet_state == 1) {
        packet_length=byte; packet_used=0;
        packet_state=packet_length ? 2 : 0;
    } else {
        packet[packet_used++]=byte;
        if (packet_used == packet_length) {
            if (packet_code == 0x86u && packet_length == 8u)
                select_geometry(packet[0]|((unsigned)packet[1]<<8),packet[2]|((unsigned)packet[3]<<8));
            packet_state=0;
        }
    }
}
static void uart_output(void *unused, uint8_t byte) {
    (void)unused;
    pthread_mutex_lock(&io_lock);
    queue_put(&vdp_to_host,byte);
    pthread_mutex_unlock(&io_lock);
    if (vdp_trace) fputc(byte,vdp_trace);
    atomic_fetch_add(&tx_count,1);
    parse_packet(byte);
}
static void debug_output(void *unused, uint8_t byte) {
    (void)unused;
    if (debug_trace) fputc(byte,debug_trace);
    if (atomic_load(&debug_logging)) fputc(byte,stderr);
}
static void publish_frame(void) {
    if (scan_columns != expected_columns || scan_rows != expected_rows ||
        crop_left+screen_width > RASTER_COLUMNS ||
        crop_top+(screen_height-1u)*scan_repeat >= RASTER_ROWS) return;
    pthread_mutex_lock(&video_lock);
    for (unsigned y=0;y<screen_height;++y) {
        const uint8_t *line=raster[crop_top+y*scan_repeat]+crop_left;
        for(unsigned x=0;x<screen_width;++x) {
            uint8_t c=line[x];
            size_t out=((size_t)y*screen_width+x)*3u;
            rgb[out]=(c&3u)*85u; rgb[out+1u]=((c>>2)&3u)*85u;
            rgb[out+2u]=((c>>4)&3u)*85u;
        }
    }
    output_width=screen_width; output_height=screen_height;
    output_rate=(float)pixel_rate/(float)(expected_columns*expected_rows);
    pthread_mutex_unlock(&video_lock);
    /* Mode changes temporarily expose incomplete DMA rasters. Only a complete
     * frame with the selected modeline may drive the eZ80's VSync signal. */
    atomic_fetch_add(&frame_count,1);
}
static void capture_vga(void *unused,int port,const uint8_t *data,size_t length,
                        uint32_t rate,uint8_t bits,uint8_t channels) {
    (void)unused;(void)port;
    if(bits!=8u || channels!=1u || (length&3u)) {fail("unsupported VGA DMA format");return;}
    pixel_rate=rate;
    for(size_t i=0;i<length;++i) {
        uint8_t c=data[i^2u];
        if(!(c&0x80u) && (previous_pixel&0x80u)) {
            if(scan_started && scan_y<RASTER_ROWS) {
                scan_rows=scan_y+1u; publish_frame();
            }
            scan_started=true; scan_y=UINT32_MAX;
        }
        if(!(c&0x40u) && (previous_pixel&0x40u)) {
            if(scan_x)scan_columns=scan_x;
            scan_x=0;scan_y++;
        }
        if(scan_started && scan_y<RASTER_ROWS && scan_x<RASTER_COLUMNS)
            raster[scan_y][scan_x]=c;
        scan_x++;previous_pixel=c;
    }
}
static void capture_audio(void *unused,int port,const uint8_t *data,size_t length,
                          uint32_t rate,uint8_t bits,uint8_t channels) {
    (void)unused;(void)port;(void)rate;
    if(bits!=16u || channels!=1u || (length&3u)) {fail("unsupported audio DMA format");return;}
    pthread_mutex_lock(&audio_lock);
    for(size_t i=0;i<length;i+=2u) {
        if(audio_size==AUDIO_CAPACITY) {audio_head=(audio_head+1u)%AUDIO_CAPACITY;audio_size--;}
        audio_ring[(audio_head+audio_size++)%AUDIO_CAPACITY]=data[(i^2u)+1u];
    }
    pthread_mutex_unlock(&audio_lock);
}
static void feed_queues(void) {
    esp32_periph_t *p=flexe_session_periph(session);
    uint8_t bytes[128];size_t n=0;
    size_t space=128u-periph_uart_rx_pending_num(p,2);
    pthread_mutex_lock(&io_lock);
    while(n<space && n<sizeof(bytes) && queue_get(&host_to_vdp,&bytes[n]))n++;
    pthread_mutex_unlock(&io_lock);
    if(n && periph_uart_rx_inject_num(p,2,bytes,n)!=n)fail("UART2 input rejected");
    space=128u-periph_uart_rx_pending_num(p,0);n=0;
    pthread_mutex_lock(&io_lock);
    while(n<space && n<sizeof(bytes) && queue_get(&keyboard,&bytes[n]))n++;
    pthread_mutex_unlock(&io_lock);
    if(n && periph_uart_rx_inject_num(p,0,bytes,n)!=n)fail("UART0 input rejected");
    atomic_store(&uart_pending,(unsigned)periph_uart_rx_pending_num(p,2));
}
/* ISYNC observes every path that wrote IRAM, including generated direct
 * stores, ROM memcpy and aliases. The shadow avoids flushing unchanged code. */
static void instruction_sync(xtensa_cpu_t *cpu,void *unused) {
    (void)unused;
    atomic_fetch_add(&instruction_syncs,1);
    const uint8_t *current=mem_get_ptr(cpu->mem,iram_start);
    if(iram_shadow && current && memcmp(current,iram_shadow,iram_length)) {
        memcpy(iram_shadow,current,iram_length);
        xtensa_invalidate_code(cpu,iram_start,iram_length);
        atomic_fetch_add(&code_invalidations,1);
    }
}
void vdp_setup(void) {
    const char *image=getenv("AGON_FLEXE_FIRMWARE");
    const char *elf=getenv("AGON_FLEXE_ELF");
    const char *engine=getenv("AGON_FLEXE_ENGINE");
    if(!image || image[0]!='/') {fprintf(stderr,"[flexe-agon-emulator] AGON_FLEXE_FIRMWARE must name an absolute .bin path\n");exit(2);}
    if(engine && strcmp(engine,"jit") && strcmp(engine,"interp")) {fprintf(stderr,"[flexe-agon-emulator] invalid AGON_FLEXE_ENGINE\n");exit(2);}
    artifacts=getenv("AGON_FLEXE_ARTIFACTS");
    host_trace=trace_open("mos-to-vdp.bin");vdp_trace=trace_open("vdp-to-mos.bin");
    delivered_trace=trace_open("vdp-to-mos-delivered.bin");
    debug_trace=trace_open("esp32-uart0.log");
    flexe_session_config_t cfg={.bin_path=image,.native_freertos=1,
        .disable_jit=engine && !strcmp(engine,"interp"),.uart_cb=debug_output};
    session=flexe_session_create(&cfg);
    if(!session){fprintf(stderr,"[flexe-agon-emulator] failed to load selected firmware\n");exit(2);}
    /* Session ELF loading installs compatibility function replacements.
     * Keep our ELF independent: symbols identify diagnostic stops only. */
    if(elf && *elf) {
        diagnostic_symbols=elf_symbols_load(elf);
        if(!diagnostic_symbols){fail("cannot load selected diagnostic ELF");return;}
        static const char *panic_names[]={"panic_abort","esp_panic_handler"};
        for(unsigned i=0;i<sizeof(panic_names)/sizeof(panic_names[0]);++i) {
            uint32_t address;
            if(!elf_symbols_find(diagnostic_symbols,panic_names[i],&address))
                for(int core=0;core<2;++core)xtensa_set_breakpoint(flexe_session_cpu(session,core),address);
        }
    }
    const flexe_target_desc_t *target=mem_target(flexe_session_mem(session));
    iram_start=target->iram_start;iram_length=target->iram_end-target->iram_start;
    /* Classic ESP32 maps executable SRAM beyond its architectural IRAM
     * descriptor. ISYNC must cover uploaded code throughout that mapping.
     * This changes no target descriptor or data-alias semantics. */
    if (target->capabilities & FLEXE_TARGET_CAP_ESP32_CLASSIC_PERIPHERALS) {
        iram_start=0x40070000u;iram_length=0x00050000u;
    }
    iram_shadow=malloc(iram_length);
    if(!iram_shadow){fail("cannot allocate instruction coherence state");return;}
    memcpy(iram_shadow,mem_get_ptr(flexe_session_mem(session),iram_start),iram_length);
    for(int core=0;core<2;++core) {
        flexe_session_cpu(session,core)->instruction_sync=instruction_sync;
        flexe_session_cpu(session,core)->instruction_sync_ctx=NULL;
    }
    esp32_periph_t *p=flexe_session_periph(session);
    agon_ps2_init(&ps2,p);
    periph_set_uart_callback_num(p,2,uart_output,NULL);
    periph_set_i2s_tx_callback(p,1,capture_vga,NULL);
    periph_set_i2s_tx_callback(p,0,capture_audio,NULL);
    if(atomic_load(&debug_logging)) fprintf(stderr,"[flexe-agon-emulator] actual Xtensa firmware=%s engine=%s\n",image,cfg.disable_jit?"interp":"jit");
}
void vdp_loop(void) {
    if(!session){fail("VDP loop entered before setup");return;}
    uint64_t wall_start=monotonic_ns(),last_flush=wall_start;
    xtensa_cpu_t *cpu=flexe_session_cpu(session,0),*other=flexe_session_cpu(session,1);
    while(!atomic_load(&stopping)) {
        /* Service the APP CPU's higher-priority RTC consumer before the
         * PRO CPU's generic RTC handler can clear shared status. The CPUs
         * otherwise execute serial host batches, unlike concurrent silicon. */
        if((mem_read32(flexe_session_mem(session),0x3ff48044)&32))
            flexe_session_run_core(session,1,BATCH);
        /* Hold Fab's CTS while native firmware initializes. Serial console is
         * enabled before admitting any eZ80 bytes, avoiding mid-command inserts. */
        if(!console_sent && cpu->cycle_count>=300000000ull) {
            static const uint8_t console[]={23,0,254,1};
            if(periph_uart_rx_inject_num(flexe_session_periph(session),2,console,sizeof(console))==sizeof(console))
                console_sent=true;
        }
        if(console_sent && cpu->cycle_count>=302400000ull) {
            if(!atomic_load(&ready) && requested_mode!=UINT32_MAX) {
                uint8_t mode[]={22,(uint8_t)requested_mode};
                periph_uart_rx_inject_num(flexe_session_periph(session),2,mode,sizeof(mode));
            }
            atomic_store(&ready,true);feed_queues();
        }
        if(cpu->debug_break || other->debug_break || cpu->breakpoint_hit || other->breakpoint_hit ||
           flexe_session_run_core(session,0,BATCH)<0) {
            fprintf(stderr,"[flexe-agon-emulator] fault PCs=%08x/%08x cause=%u/%u\n",cpu->pc,other->pc,cpu->exccause,other->exccause);
            fail("ESP32 stopped or faulted");break;
        }
        flexe_session_post_batch(session,BATCH);
        /* ULP is a separate 8 MHz processor, executing the program loaded by
         * firmware. The external keyboard drives real modeled RTC GPIO pads. */
        pthread_mutex_lock(&io_lock);
        while(key_size) {
            agon_ps2_event(&ps2,key_events[key_head].code,key_events[key_head].down,ulp.ticks/8);
            key_head=(key_head+1)%4096;key_size--;
        }
        pthread_mutex_unlock(&io_lock);
        while(ulp.ticks<cpu->cycle_count/30) {
            agon_ps2_tick(&ps2,flexe_session_periph(session),ulp.ticks/8);
            unsigned ticks=agon_ulp_step(&ulp,flexe_session_mem(session),flexe_session_periph(session));
            if(!ticks || ps2.fault) {fail("ULP/PS2 execution failed");break;}
            ulp.ticks+=ticks;
        }
        atomic_store(&cycle_count,cpu->cycle_count);
        atomic_store(&retired_count,cpu->insn_count+other->insn_count);
        /* Never run the ESP32 clock faster than real time; slow hosts simply
         * run slower. No additional sleeps or fake FreeRTOS scheduling. */
        uint64_t now=monotonic_ns();
        if(now-last_flush>=250000000ull) {
            pthread_mutex_lock(&io_lock);
            if(host_trace)fflush(host_trace);
            if(delivered_trace)fflush(delivered_trace);
            pthread_mutex_unlock(&io_lock);
            if(vdp_trace)fflush(vdp_trace);
            if(debug_trace)fflush(debug_trace);
            last_flush=now;
        }
        uint64_t elapsed=now-wall_start;
        uint64_t target=agon_cycles_to_ns(cpu->cycle_count);
        if(target>elapsed+1000000ull)delay_ns(1000000ull);
    }
    atomic_store(&ready,false);
    if(atomic_load(&debug_logging)) fprintf(stderr,"[flexe-agon-emulator] ULP pc=%u instructions=%llu start=%08x PS2 sent=%llu commands=%llu pending=%u/%u\n",
        ulp.pc,(unsigned long long)ulp.instructions,mem_read32(flexe_session_mem(session),0x3ff4882c),
        (unsigned long long)ps2.sent,(unsigned long long)ps2.received,ps2.size,ps2.rsize);
    FILE *frame=trace_open("final.ppm");
    if(frame){pthread_mutex_lock(&video_lock);fprintf(frame,"P6\n%u %u\n255\n",output_width,output_height);
        fwrite(rgb,3,(size_t)output_width*output_height,frame);pthread_mutex_unlock(&video_lock);fclose(frame);}
    FILE *stats=trace_open("stats.json");
    if(stats){fprintf(stats,"{\"engine\":\"%s\",\"frames\":%llu,\"cycles\":%llu,\"retired\":%llu,\"uart_rx\":%llu,\"uart_tx\":%llu,\"uart_delivered\":%llu,\"isync\":%llu,\"code_invalidations\":%llu,\"keys\":%llu,\"failed\":%s,\"unhandled\":%d,\"unregistered\":%d,\"unmapped\":%llu}\n",
        flexe_session_jit(session)?"jit":"interp",(unsigned long long)atomic_load(&frame_count),
        (unsigned long long)cpu->cycle_count,(unsigned long long)(cpu->insn_count+other->insn_count),
        (unsigned long long)atomic_load(&rx_count),(unsigned long long)atomic_load(&tx_count),
        (unsigned long long)atomic_load(&delivered_count),
        (unsigned long long)atomic_load(&instruction_syncs),(unsigned long long)atomic_load(&code_invalidations),
        (unsigned long long)atomic_load(&key_count),atomic_load(&failed)?"true":"false",
        periph_unhandled_count(flexe_session_periph(session)),rom_stubs_unregistered_count(flexe_session_rom(session)),
        (unsigned long long)mem_unmapped_count(flexe_session_mem(session)));fclose(stats);}
    pthread_mutex_lock(&io_lock);
    if(host_trace){fclose(host_trace);host_trace=NULL;}
    if(delivered_trace){fclose(delivered_trace);delivered_trace=NULL;}
    pthread_mutex_unlock(&io_lock);
    if(vdp_trace)fclose(vdp_trace);
    if(debug_trace)fclose(debug_trace);
    flexe_session_destroy(session);session=NULL;
    free(iram_shadow);iram_shadow=NULL;
    elf_symbols_destroy(diagnostic_symbols);diagnostic_symbols=NULL;
}
void vdp_shutdown(void){atomic_store(&stopping,true);emu_app_running=0;}
bool z80_uart0_is_cts(void){pthread_mutex_lock(&io_lock);bool cts=atomic_load(&ready) && !atomic_load(&failed) &&
    /* The real driver's FIFO-full threshold can be 120 bytes. Stopping at a
     * smaller arbitrary watermark deadlocks when RX timeout IRQs are disabled.
     * Match the modeled 128-byte hardware FIFO and count queued bytes too. */
    host_to_vdp.size+atomic_load(&uart_pending)<128u;pthread_mutex_unlock(&io_lock);return cts;}
void z80_send_to_vdp(uint8_t byte){pthread_mutex_lock(&io_lock);queue_put(&host_to_vdp,byte);
    if(host_trace)fputc(byte,host_trace);
    pthread_mutex_unlock(&io_lock);atomic_fetch_add(&rx_count,1);}
bool z80_recv_from_vdp(uint8_t *byte){pthread_mutex_lock(&io_lock);bool got=queue_get(&vdp_to_host,byte);
    if(got){atomic_fetch_add(&delivered_count,1);if(delivered_trace)fputc(*byte,delivered_trace);}
    pthread_mutex_unlock(&io_lock);return got;}
void copyVgaFramebuffer(uint32_t *width,uint32_t *height,uint8_t *buffer,float *rate){pthread_mutex_lock(&video_lock);
    *width=output_width;*height=output_height;*rate=output_rate;memcpy(buffer,rgb,(size_t)*width**height*3u);pthread_mutex_unlock(&video_lock);}
/* The firmware already receives real modeled I2S VSync interrupts. Fab's
 * host-presentation callback must not inject another interrupt into ESP32. */
void signal_vblank(void){}
void set_startup_screen_mode(uint32_t mode){requested_mode=mode;}
void setVdpDebugLogging(bool enabled){atomic_store(&debug_logging,enabled);flexe_set_debug_logging(enabled);}
void getAudioSamples(uint8_t *buffer,uint32_t length){pthread_mutex_lock(&audio_lock);
    for(uint32_t i=0;i<length;++i){if(audio_size){buffer[i]=audio_ring[audio_head];audio_head=(audio_head+1u)%AUDIO_CAPACITY;audio_size--;}else buffer[i]=127;}
    pthread_mutex_unlock(&audio_lock);}
static void enqueue_key(uint8_t byte){pthread_mutex_lock(&io_lock);queue_put(&keyboard,byte);pthread_mutex_unlock(&io_lock);atomic_fetch_add(&key_count,1);}
void flexe_inject_ascii(uint8_t byte){enqueue_key(byte);}
void sendPS2KbEventToFabgl(uint16_t code,uint8_t down){
    pthread_mutex_lock(&io_lock);
    if(key_size==4096) fail("keyboard event queue exhausted");
    else {
        unsigned at=(key_head+key_size++)%4096;
        key_events[at].code=code;key_events[at].down=down;
        atomic_fetch_add(&key_count,1);
    }
    pthread_mutex_unlock(&io_lock);
}
/* Fab's --osk virtual keys are intentionally not guessed: use ordinary SDL
 * scancodes, whose PS/2 device path preserves transitions. */
void sendVKeyEventToFabgl(uint32_t key,uint8_t down){(void)key;(void)down;}
void sendHostMouseEventToFabgl(const uint8_t *packet){(void)packet;}
void dump_vdp_mem_stats(void){fprintf(stderr,"[flexe-agon-emulator] cycles=%llu retired=%llu frames=%llu rx=%llu tx=%llu delivered=%llu pending=%u failed=%d\n",
    (unsigned long long)atomic_load(&cycle_count),(unsigned long long)atomic_load(&retired_count),
    (unsigned long long)atomic_load(&frame_count),(unsigned long long)atomic_load(&rx_count),
    (unsigned long long)atomic_load(&tx_count),(unsigned long long)atomic_load(&delivered_count),
    atomic_load(&uart_pending),atomic_load(&failed));}
uint64_t flexe_vdp_frames(void){return atomic_load(&frame_count);}
uint64_t flexe_vdp_cycles(void){return atomic_load(&cycle_count);}
bool flexe_vdp_failed(void){return atomic_load(&failed);}

uint64_t flexe_vdp_instruction_syncs(void){return atomic_load(&instruction_syncs);}
uint64_t flexe_vdp_code_invalidations(void){return atomic_load(&code_invalidations);}
