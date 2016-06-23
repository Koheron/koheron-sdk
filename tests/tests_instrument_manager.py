# Unit tests for InstrumentManager

import os
import pytest

from instrument_manager import InstrumentManager

host = os.getenv('HOST', '192.168.1.100')
im = InstrumentManager(host)

class TestsInstrumentManager:
    def test_get_app_version(self):
        version = im.get_app_version()
        assert 'date' in version
        assert 'machine' in version
        assert 'time' in version
        assert 'user' in version
        assert 'version' in version

    def test_get_bistream_id(self):
        assert len(im.get_bistream_id()) == 64

    def test_remove_and_restore(self):
        """ Removes all local instruments and restore backup"""
        local_instruments = im.get_local_instruments()

        for instrum in local_instruments:
            if len(local_instruments[instrum]) > 0:
                for version in local_instruments[instrum]:
                    im.remove_local_instrument(instrum, version)
                    # Check the instrument has been deleted
                    new_instruments = im.get_local_instruments()
                    assert version not in new_instruments[instrum]

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