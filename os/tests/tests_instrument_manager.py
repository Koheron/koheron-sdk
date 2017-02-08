# Unit tests for InstrumentManager
import os

from instrument_manager import InstrumentManager
from koheron import KoheronClient, Common

host = os.getenv('HOST', '192.168.1.100')
im = InstrumentManager(host)

client = KoheronClient(host)
common = Common(client)

class TestsInstrumentManager:
    def test_get_app_version(self):
        version = im.get_app_version()
        assert 'date' in version
        assert 'machine' in version
        assert 'time' in version
        assert 'user' in version
        assert 'version' in version

    def test_get_server_version(self):
        assert im.get_server_version() == common.get_server_version()

    def test_get_bitstream_id(self):
        assert im.get_bitstream_id() == common.get_bitstream_id()

    def test_get_dna(self):
        assert im.get_dna() == str(common.get_dna())

    def test_get_board_version(self):
        assert 'koheron-sdk' in im.get_board_version()

    def test_remove_and_restore(self):
        ''' Removes all local instruments and restore backup '''
        local_instruments = im.get_local_instruments()

        for instrum in local_instruments:
            for version in local_instruments[instrum]:
                im.remove_local_instrument(instrum, version)
                # Check the instrument has been deleted
                new_instruments = im.get_local_instruments()
                assert instrum not in new_instruments or version not in new_instruments[instrum]

        im.restore_backup()
        assert len(im.get_local_instruments()) > 0

    def test_install_instruments(self):
        local_instruments = im.get_local_instruments()
        dnas = []

        for instrum in local_instruments:
            for version in local_instruments[instrum]:
                im.deploy_local_instrument(instrum, version)
                live_instrum = im.get_live_instrument()
                dnas.append(im.get_dna())
                assert dnas[0] == dnas[-1] # DNA is independent of the instrument
                assert live_instrum['name'] == instrum
                assert live_instrum['sha'] == version


tests = TestsInstrumentManager()
tests.test_get_app_version()
