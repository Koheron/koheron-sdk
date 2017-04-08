import click

# --------------------------------------------
# Call koheron-server
# --------------------------------------------

class ConnectionType(object):
    def __init__(self, host="", unixsock=""):
        self.host = host

@click.group()
@click.option('--host', default='', help='Host ip address', envvar='HOST')
@click.pass_context
def cli(ctx, host):
    if host != "":
        ctx.obj = ConnectionType(host=str(host))

@cli.command()
def version():
    ''' Get the version of koheron python library '''
    from .version import __version__
    click.echo(__version__)

@cli.command()
@click.pass_obj
def devices(conn_type):
    ''' Get the list of devices '''
    from .koheron import KoheronClient
    client = KoheronClient(host=conn_type.host)
    click.echo(client.devices_idx)

@cli.command()
@click.pass_obj
@click.option('--device', default=None)
def commands(conn_type, device):
    ''' Get the list of commands for a specified device '''
    from .koheron import KoheronClient
    client = KoheronClient(host=conn_type.host)
    if device is None:
        click.echo(client.commands)
    else:
        device_idx = client.devices_idx[device]
        click.echo(client.commands[device_idx])

# --------------------------------------------
# Call HTTP API
# --------------------------------------------

@cli.command()
@click.pass_obj
@click.argument('instrument_zip')
@click.option('--run', is_flag=True)
def upload(conn_type, instrument_zip, run):
    ''' Upload instrument.zip '''
    from .koheron import upload_instrument
    upload_instrument(conn_type.host, instrument_zip, run=run)

@cli.command()
@click.pass_obj
@click.argument('instrument_name', required=False)
@click.option('--restart', is_flag=True)
def run(conn_type, instrument_name, restart):
    ''' Run a given instrument '''
    from .koheron import run_instrument
    run_instrument(conn_type.host, instrument_name, restart=restart)
