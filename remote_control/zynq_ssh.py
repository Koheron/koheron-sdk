"""
SSH control of the Zynq
"""

# Use the Paramiko package for native Python SSH
# http://jessenoller.com/blog/2009/02/05/ssh-programming-with-paramiko-completely-different
import paramiko
import os


class ZynqSSH:
    """ Toolbox to control the Zynq via SSH

    Execute commands, upload files, program the PL,
    """


    def __init__(self, tcp_ip_, password_):
        """ Initialize a SSH connection with the Zynq

        Args:
            tcp_ip_: The IP address of the Zynq
            password_: Superuser password
        """
        if type(tcp_ip_) != str:
            raise TypeError("IP address must be a string")

        if type(password_) != str:
            raise TypeError("Password must be a string")

        self.ip = tcp_ip_
        self.password = password_
        self.ssh = paramiko.SSHClient()
        self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self.ssh.connect(tcp_ip_, username='root', password=password_)
        return

    # -----------------------------------------------
    # Basic functions
    # -----------------------------------------------

    def run(self, cmd):
        """ Run a command on the host

        Args:
            cmd: String of the command. Example: "ls -l /usr"

        Returns:
            self.stdin, self.stdout and self.stderr
        """
        self.stdin, self.stdout, self.stderr = self.ssh.exec_command(cmd)

        # Wait for the execution to finish
        while not self.stdout.channel.exit_status_ready():
            continue

        return

    def get_stdout(self):
        """ Return a array with the lines of the standard output
        """
        return self.stdout.readlines()

    def upload(self, filename, remote_dest):
        """ Upload a file to the host

        Use the SFTP protocol to upload a file.

        Args:
            filename: Name and path of the file to upload
            remote_dest: Remote path where the file must be uploaded
        """
        # Open SFTP if used for the first time
        if not hasattr(self, 'ftp'):
            self.ftp = self.ssh.open_sftp()

        remote_filename = os.path.join(remote_dest, os.path.basename(filename))
        remote_filename = remote_filename.replace('\\', '/')

        self.ftp.put(filename, remote_filename)
        return

    def download(self, remote_filename, local_dest):
        """ Download a file

        Used the SFTP protocol to download a file.

        Args:
            remote_filename: Name and path of the file
                             to download on the remote machine
            local_dest: Path where the file must be saved
                        on the local machine
        """
        # Open SFTP if used for the first time
        if not hasattr(self, 'ftp'):
            self.ftp = self.ssh.open_sftp()

        local_f = os.path.join(local_dest, os.path.basename(remote_filename))
        self.ftp.get(remote_filename, local_f)
        return
		
    # -----------------------------------------------
    # Instruments
    # -----------------------------------------------

    def unzip_app(self):
        self.run('unzip -o /usr/local/flask/app.zip -d /usr/local/flask')

    def get_local_instruments(self):
        self.run('ls /usr/local/instruments/backup')
        instruments = {}
        for line in self.get_stdout():
            tokens =line.split('.')[0].split('-')
            instruments[tokens[0]] = [tokens[1]]
        return instruments

    def install_instrument(self, instrument_name):
        instruments = self.get_local_instruments()
        if self.get_local_instruments():
            for name, shas in instruments.items():
                if name == instrument_name and len(shas) > 0:
                    zip_filename = '/usr/local/instruments/backup/' + name + '-' + shas[0] + '.zip'
                    self.run('bash /usr/local/flask/stack/install_instrument.sh ' + zip_filename + ' ' + name)
                    return
        print("Instrument " + instrument_name + " not found")

    # -----------------------------------------------
    # Load bitstream
    # -----------------------------------------------

    def load_pl(self, bitfilename):
        """ Program the Zynq PL

        Load a bitstream into the Zynq PL using the Xilinx xdevcfg driver,
        and check if the programmation was successful.

        Args:
            bitfilename: Name and path of bistream file on local machine
        """
        if os.path.splitext(bitfilename)[1] != '.bit':
            raise ValueError("Not a bitstream file")

        # Load the xdevcfg device if not in /dev
        if not self._is_xdevcfg():
            self.rp_ssh._create_xdevcfg_dev()

        # Load bitfile from host
        self.upload(bitfilename, '/tmp')

        # Call xdevcfg to program the bitstream into the PL
        basename = os.path.basename(bitfilename)
        self.run('cat /tmp/' + basename + ' > /dev/xdevcfg')

        # Check whether PL programmation was successful
        self.run('cat /sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done')

        var = self.get_stdout()
        if var[0] != '1\n':
            raise RuntimeError("PL programmation failed")

        # Remove bitfile from host
        self.run('rm /tmp/' + basename)

        return

    def _is_xdevcfg(self):
        """ Check whether the xdevcfg driver is loaded
        """
        self.run('ls /dev')
        res = False

        for driver_name in self.get_stdout():
            if driver_name.strip('\n') == "xdevcfg":
                res = True
                break

        return res

    def _create_xdevcfg_dev(self):
        """ Load the xdevcfg device
        """
        self.run('mknod /dev/xdevcfg c 259 0')

        stdin, stdout, stderr = self.ssh.exec_command('sudo chmod 666 /dev/xdevcfg')

        stdin.write(self.password+'\n')
        stdin.flush()

        if not self._is_xdevcfg():
            raise RuntimeError("Cannot load /dev/xdevcfg")

        return

    def __del__(self):
        if hasattr(self, 'ftp'):
            self.ftp.close()

        return