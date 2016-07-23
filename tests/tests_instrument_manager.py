# Unit tests for InstrumentManager
import context
import os
import pytest

from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient
from drivers.common import Common

host = os.getenv('HOST', '192.168.1.100')
im = InstrumentManager(host)

client = KClient(host)
common = Common(client)

class TestsInstrumentManager:
    def test_get_app_version(self):
        version = im.get_app_version()
        assert 'date' in version
        assert 'machine' in version
        assert 'time' in version
        assert 'user' in version
        assert 'version' in version

    def test_get_bistream_id(self):
        assert im.get_bistream_id() == common.get_bitstream_id()

    def test_get_dna(self):
        assert im.get_dna() == common.get_dna()

    def test_get_board_version(self):
        assert 'zynq-sdk' in im.get_board_version()

    def test_remove_and_restore(self):
        """ Removes all local instruments and restore backup"""
        local_instruments = im.get_local_instruments()

        for instrum in local_instruments:
            if len(local_instruments[instrum]) > 0:
                for version in local_instruments[instrum]:
                    im.remove_local_instrument(instrum, version)
                    # Check the instrument has been deleted
                    new_instruments = im.get_local_instruments()
                    assert instrum not in new_instruments or version not in new_instruments[instrum]

        im.restore_backup()
        assert len(im.get_local_instruments()) > 0

    def test_install_instruments(self):
        local_instruments = im.get_local_instruments()

        for instrum in local_instruments:
            if len(local_instruments[instrum]) > 0:
                for version in local_instruments[instrum]:
                    im.deploy_local_instrument(instrum, version)
                    curr_instrum = im.get_current_instrument()
                    assert curr_instrum['name'] == instrum
                    assert curr_instrum['sha'] == version

tests = TestsInstrumentManager()
tests.test_get_app_version()
# tests.test_remove_and_restore()
# tests.test_install_instruments()