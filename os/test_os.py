# Test to be executed with ALPHA250 running FFT instrument

import sys
import paramiko
import unittest
import pprint
import urllib.request
import json
from koheron import connect

def parse_systemctl_line(line):
    return line.split(']: ')[1].split('\n')[0]

def set_ssh(host):
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(host, username='root', password='changeme')
    return ssh

class TestSystem(unittest.TestCase):
    def __init__(self, name, ssh):
        super(TestSystem, self).__init__(name)
        self.ssh = ssh

    def test_ifconfig(self):
        stdin, stdout, sterr = self.ssh.exec_command('ifconfig')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertTrue(lines[0].startswith("eth0"))
        self.assertEqual(lines[1].lstrip(), "inet {}  netmask 255.255.255.0  broadcast 192.168.1.255\n".format(host))

    def test_ethernet_speed(self):
        stdin, stdout, sterr = self.ssh.exec_command('cat /sys/class/net/eth0/speed')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertEqual(lines[0], "1000\n")

    def test_i2c_devices(self):
        stdin, stdout, sterr = self.ssh.exec_command('ls /sys/bus/i2c/devices')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertTrue("i2c-0\n" in lines)
        self.assertTrue("i2c-1\n" in lines)

    def test_spi_devices(self):
        stdin, stdout, sterr = self.ssh.exec_command('ls /sys/bus/spi/devices')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertTrue("spi1.0\n" in lines)
        self.assertTrue("spi2.0\n" in lines)

    def test_cpuinfo(self):
        stdin, stdout, sterr = self.ssh.exec_command('cat /proc/cpuinfo | grep Hardware')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertEqual(lines[0].split(': ')[1].split('\n')[0], "Xilinx Zynq Platform")

    def test_lib_firmware(self):
        stdin, stdout, sterr = self.ssh.exec_command('ls /lib/firmware/')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertTrue("fft.bit.bin\n" in lines)
        self.assertTrue("pl.dtbo\n" in lines)

class TestKoheronServer(unittest.TestCase):
    def __init__(self, name, ssh):
        super(TestKoheronServer, self).__init__(name)
        self.ssh = ssh

    def test_koheron_server_service(self):
        stdin, stdout, sterr = self.ssh.exec_command('systemctl status koheron-server')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertEqual(lines[0], "● koheron-server.service - Koheron TCP/Websocket server\n")
        self.assertEqual(lines[1].lstrip(), "Loaded: loaded (/etc/systemd/system/koheron-server.service; disabled; vendor preset: enabled)\n")
        self.assertTrue(lines[2].lstrip().startswith("Active: active (running)"))

    def test_koheron_server_ready(self):
        stdin, stdout, sterr = self.ssh.exec_command('journalctl -u koheron-server | grep Koheron | grep ready')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertEqual(parse_systemctl_line(lines[0]), "Koheron server ready")

    def test_fpga_bistream_loaded(self):
        stdin, stdout, sterr = self.ssh.exec_command('journalctl -u koheron-server | grep FpgaManager | grep loaded')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertEqual(parse_systemctl_line(lines[0]), "FpgaManager: Bitstream successfully loaded")

    def test_ethernet_interface_found(self):
        stdin, stdout, sterr = self.ssh.exec_command('journalctl -u koheron-server | grep eth0')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertEqual(parse_systemctl_line(lines[0]), "Interface eth0 found: {}".format(host))

class TestUwsgi(unittest.TestCase):
    def __init__(self, name, ssh):
        super(TestUwsgi, self).__init__(name)
        self.ssh = ssh

    def test_uwsgi_service(self):
        stdin, stdout, sterr = self.ssh.exec_command('systemctl status uwsgi')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertEqual(lines[0], "● uwsgi.service - uWSGI\n")
        self.assertEqual(lines[1].lstrip(), "Loaded: loaded (/etc/systemd/system/uwsgi.service; enabled; vendor preset: enabled)\n")
        self.assertTrue(lines[2].lstrip().startswith("Active: active (running)"))

    def test_intruments_details(self):
        with urllib.request.urlopen("http://{}/api/instruments/details".format(host)) as url:
            inst_details = json.loads(url.read())
            # pprint.pprint(inst_details)
    
            self.assertTrue("instruments" in inst_details)
            self.assertGreaterEqual(len(inst_details["instruments"]), 1)
            inst0 = inst_details["instruments"][0]
            self.assertTrue("is_default" in inst0)
            self.assertTrue(type(inst0["is_default"]) == type(True)) # Check is boolean
            self.assertTrue("name" in inst0)
            self.assertTrue("version" in inst0)
            self.assertEqual(len(inst0["version"].split('.')), 3)

            self.assertTrue("live_instrument" in inst_details)
            live_inst = inst_details["live_instrument"]
            self.assertTrue("is_default" in live_inst)
            self.assertTrue(type(live_inst["is_default"]) == type(True)) # Check is boolean
            self.assertTrue("name" in live_inst)
            self.assertTrue("version" in live_inst)
            self.assertEqual(len(live_inst["version"].split('.')), 3)

class TestNginx(unittest.TestCase):
    def __init__(self, name, ssh):
        super(TestNginx, self).__init__(name)
        self.ssh = ssh

    def test_nginx_service(self):
        stdin, stdout, sterr = self.ssh.exec_command('systemctl status nginx')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertEqual(lines[0], "● nginx.service - A high performance web server and a reverse proxy server\n")
        self.assertEqual(lines[1].lstrip(), "Loaded: loaded (/etc/systemd/system/nginx.service; enabled; vendor preset: enabled)\n")
        self.assertTrue(lines[2].lstrip().startswith("Active: active (running)"))

    def test_koheron_location_folder(self):
        stdin, stdout, sterr = self.ssh.exec_command('ls /usr/local/www')
        lines = stdout.readlines()
        # pprint.pprint(lines)
        self.assertTrue("bootstrap.min.js\n" in lines)
        self.assertTrue("glyphicons-halflings-regular.woff2\n" in lines)
        self.assertTrue("html-imports.min.js\n" in lines)
        self.assertTrue("html-imports.min.js.map\n" in lines)
        self.assertTrue("index.html\n" in lines)
        self.assertTrue("instruments.js\n" in lines)
        self.assertTrue("jquery.min.js\n" in lines)
        self.assertTrue("kbird.ico\n" in lines)
        self.assertTrue("koheron.css\n" in lines)
        self.assertTrue("koheron.svg\n" in lines)
        self.assertTrue("koheron_logo.svg\n" in lines)
        self.assertTrue("lato-v11-latin-400.woff2\n" in lines)
        self.assertTrue("lato-v11-latin-700.woff2\n" in lines)
        self.assertTrue("lato-v11-latin-900.woff2\n" in lines)
        self.assertTrue("main.css\n" in lines)
        self.assertTrue("navigation.html\n" in lines)
        self.assertTrue("version.json\n" in lines)

    def test_koheron_version(self):
        with urllib.request.urlopen("http://{}/koheron/version.json".format(host)) as url:
            version_dict = json.loads(url.read())
            self.assertTrue("version" in version_dict)
            self.assertEqual(len(version_dict["version"].split('.')), 3)

def run_test_suite(test_suite, ssh):
    test_names = unittest.TestLoader().getTestCaseNames(test_suite)
    suite = unittest.TestSuite()
    [suite.addTest(test_suite(test_name, ssh)) for test_name in test_names]
    unittest.TextTestRunner(verbosity=2).run(suite)

if __name__ == '__main__':
    host = sys.argv[1]
    client = connect(host, 'fft', restart=True)
    ssh = set_ssh(host)
    run_test_suite(TestSystem, ssh)
    run_test_suite(TestKoheronServer, ssh)
    run_test_suite(TestUwsgi, ssh)
    run_test_suite(TestNginx, ssh)