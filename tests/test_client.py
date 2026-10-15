"""Offline regressionstester; öppnar aldrig en serieport."""
import importlib.util
import itertools
from pathlib import Path
import unittest
from unittest.mock import patch
spec = importlib.util.spec_from_file_location('c2client', Path(__file__).parents[1] / 'cli' / 'c2prog.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
INFO = b'OK dev=09 rev=01 name=C8051F320/F321 flash=15872 page=512 lock=3DFF known=1\n'
BANNER = b'OK c2prog 1 ck=6 d=5\n'
class SerialFake:
    def __init__(self, *lines): self.lines = list(lines); self.sent = []
    def reset_input_buffer(self): pass
    def write(self, data): self.sent.append(data)
    def readline(self): return self.lines.pop(0) if self.lines else b''
class ClientTests(unittest.TestCase):
    def setUp(self):
        counter = itertools.count()
        self.clock = patch.object(c.time, 'time', side_effect=lambda: next(counter) * 0.25)
        self.clock.start()
        self.addCleanup(self.clock.stop)
    def expect_error(self, lines, code, method='info'):
        serial = SerialFake(*lines)
        with self.assertRaises(c.C2Error) as caught: getattr(c.Programmer(serial), method)()
        self.assertEqual(caught.exception.code, code)
        if method=='info': self.assertEqual(serial.sent, [b'INFO\n'])
    def test_valid_info(self): self.assertEqual(c.Programmer(SerialFake(INFO)).info()['dev'], 9)
    def test_unknown_info(self):
        result = c.Programmer(SerialFake(b'OK dev=42 rev=00 name=unknown flash=0 page=0 lock=0000 known=0\n')).info()
        self.assertFalse(result['known'])
    def test_late_banner(self):
        serial = SerialFake(BANNER, INFO)
        self.assertEqual(c.Programmer(serial).info()['dev'], 9)
        self.assertEqual(serial.sent, [b'INFO\n'])
    def test_banner_only(self): self.expect_error([BANNER], 'BADREPLY')
    def test_missing_fields(self):
        for reply in (b'OK\n', b'OK rev=01\n', INFO.replace(b' dev=09', b'')):
            with self.subTest(reply=reply): self.expect_error([reply], 'BADREPLY')
    def test_invalid_values(self):
        for reply in (INFO.replace(b'dev=09', b'dev=GG'), INFO.replace(b'dev=09', b'dev=100'), INFO.replace(b'known=1', b'known=x')):
            with self.subTest(reply=reply): self.expect_error([reply], 'BADREPLY')
    def test_prefix_and_junk(self):
        for reply in (b'OKAY dev=09\n', b'ERROR broken\n', b'garbage\n', b'ERR\n', b'\xff\n'):
            with self.subTest(reply=reply): self.expect_error([reply], 'BADREPLY')
    def test_stage_errors_preserved(self):
        for code in ('CKLOW', 'CKUNSTABLE', 'RESET', 'COMM', 'NOLINK'):
            with self.subTest(code=code): self.expect_error([f'ERR {code} detail\n'.encode()], code)
    def test_usb_timeout(self): self.expect_error([], 'TIMEOUT')
    def test_bad_status(self): self.expect_error([b'OK pages=x\n'], 'BADREPLY', 'status')
    def test_handshake(self):
        serial=SerialFake(b'boot log\n', BANNER); p=c.Programmer(serial);p.handshake()
        self.assertEqual(p.banner, {'ck':'6','d':'5'})
    def test_wrong_handshake(self):
        for reply in (b'OK\n', INFO, b'OK c2prog 1 ck=x d=5\n'):
            with self.subTest(reply=reply):self.expect_error([reply], 'BADREPLY', 'handshake')
    def test_malformed_late_banner(self):self.expect_error([b'OK c2prog 1 d=5\n'], 'BADREPLY')
if __name__ == '__main__': unittest.main()
