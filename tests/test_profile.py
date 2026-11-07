"""Synthetic launcher/identity tests. No firmware, SDL or emulator executes."""
import argparse
import hashlib
import importlib.util
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('profile_tool', Path(__file__).resolve().parents[1] / 'scripts/create_profile.py')
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)
SYMBOLS = '1 T flexe_vdp_frames\n2 T flexe_vdp_failed\n'


class Profile(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.args = argparse.Namespace(output=self.root / 'profile', firmware_elf=None,
            elf_sha256=None, library_dir=None, library_sha256=None,
            firmware_source='https://example.org/firmware/source')
        for attr in ['runtime', 'backend', 'mos', 'mos_map', 'firmware']:
            p = self.root / attr
            p.write_bytes(attr.encode())
            setattr(self.args, attr, p)
        self.args.runtime.write_text('#!/bin/sh\nprintf "SYNTHETIC:%s\\n" "$AGON_FLEXE_FIRMWARE"\nprintf "ARG:%s\\n" "$@"\n')
        self.args.firmware_sha256 = TOOL.digest(self.args.firmware)
        for name, value in [('MOS_SHA256',TOOL.digest(self.args.mos)),('MAP_SHA256',TOOL.digest(self.args.mos_map))]:
            mock = patch.object(TOOL,name,value);mock.start();self.addCleanup(mock.stop)
        for name, value in [('inspect_elf',None),('check_output',SYMBOLS)]:
            owner = TOOL if name == 'inspect_elf' else TOOL.subprocess
            mock = patch.object(owner,name,return_value=value);mock.start();self.addCleanup(mock.stop)

    def make(self):
        return TOOL.create(self.args)

    def launch(self, *args, cwd=None, engine='jit'):
        env = dict(os.environ, AGON_FLEXE_ENGINE=engine)
        return subprocess.run([str(self.args.output / 'flexe-agon-emulator'), *args],
            cwd=cwd or self.args.output, env=env, text=True, capture_output=True)

    def test_generator_does_not_launch(self):
        p = self.make()
        self.assertFalse((p/'flexe-agon-emulator').is_symlink())
        self.assertEqual(os.readlink(p/'flexe-agon-emulator.bin'),'runtime/flexe-agon-emulator')
        self.assertFalse(list(p.glob('fab-agon-emulator*')))
        self.assertFalse((p/'last-run').exists())

    def test_wrapper_relocates_with_explicit_inputs(self):
        p = self.make(); moved=self.root/'moved profile'
        p.rename(moved);self.args.output=moved
        r=self.launch('--verbose')
        self.assertEqual(r.returncode,0,r.stderr)
        self.assertIn('SYNTHETIC:'+str(moved/'esp32-firmware.bin'),r.stdout)
        self.assertIn('ARG:'+str(moved/'flexe-vdp.so'),r.stdout)
        self.assertIn('ARG:--verbose',r.stdout)

    def test_tampered_runtime_refused(self):
        p=self.make();(p/'runtime/flexe-agon-emulator').write_text('bad')
        self.assertNotEqual(self.launch().returncode,0)

    def test_tampered_mos_refused(self):
        p=self.make();(p/'mos_platform.bin').write_bytes(b'bad')
        self.assertNotEqual(self.launch().returncode,0)

    def test_wrong_backend_marker_refused(self):
        p=self.make();(p/'.bespoke-vdp-profile').write_text('native.so\n')
        self.assertNotEqual(self.launch().returncode,0)

    def test_override_forms_refused(self):
        self.make()
        for value in ['--vdp','--vdp=other.so','--mos=other','--sdcard-img=x','--firmware','--prefix']:
            with self.subTest(value=value):self.assertNotEqual(self.launch(value).returncode,0)

    def test_wrong_directory_and_engine_refused(self):
        self.make()
        self.assertNotEqual(self.launch(cwd=self.root).returncode,0)
        self.assertNotEqual(self.launch(engine='other').returncode,0)

    def test_existing_output_preserved(self):
        self.args.output.mkdir();sentinel=self.args.output/'keep';sentinel.write_text('unchanged')
        with self.assertRaises(ValueError):self.make()
        self.assertEqual(sentinel.read_text(),'unchanged')

    def test_bad_input_identity_refused(self):
        self.args.firmware_sha256='0'*64
        with self.assertRaises(ValueError):self.make()
        self.assertFalse(self.args.output.exists())

    def test_native_backend_rejected(self):
        with patch.object(TOOL.subprocess,'check_output',return_value='1 T native_backend\n'):
            with self.assertRaises(ValueError):self.make()
        self.assertFalse(self.args.output.exists())

    def test_optional_elf_requires_hash(self):
        self.args.firmware_elf=self.args.firmware
        with self.assertRaises(ValueError):self.make()

    def test_host_libraries_pinned(self):
        libs=self.root/'lib';libs.mkdir();source=libs/'libexample.so';source.write_bytes(b'synthetic')
        sums=self.root/'lib.sha256';sums.write_text(TOOL.digest(source)+'  libexample.so\n')
        self.args.library_dir=libs;self.args.library_sha256=sums
        p=self.make();self.assertEqual(self.launch().returncode,0)
        (p/'lib/libexample.so').write_bytes(b'changed')
        self.assertNotEqual(self.launch().returncode,0)

    def test_raw_link_target_pinned(self):
        p=self.make();(p/'flexe-agon-emulator.bin').unlink()
        (p/'flexe-agon-emulator.bin').symlink_to(self.args.runtime)
        self.assertNotEqual(self.launch().returncode,0)


class ElfIdentity(unittest.TestCase):
    def test_actual_elf_parser_refuses_wrong_architecture(self):
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp)/'fake-elf'
            data=bytearray(20);data[:6]=b'\x7fELF\x02\x01';data[16:18]=(3).to_bytes(2,'little')
            data[18:20]=(0).to_bytes(2,'little');p.write_bytes(data)
            with self.assertRaises(ValueError):TOOL.inspect_elf(p,shared=True)


if __name__ == '__main__':unittest.main()
