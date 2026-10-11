import click

# --------------------------------------------
# Call koheron-server
# --------------------------------------------

class ConnectionType(object):
    def __init__(self, host="", unixsock=""):
        self.host = host


def board_host(conn_type):
    if conn_type is None or not conn_type.host:
        raise click.UsageError('Specify --host or set HOST')
    return conn_type.host

@click.group()
@click.option('--host', default='', help='Board IP or hostname; deploy/status/logs also accept an HTTP(S) origin.', envvar='HOST')
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
    client = KoheronClient(host=board_host(conn_type))
    click.echo(client.devices_idx)

@cli.command()
@click.pass_obj
@click.option('--device', default=None)
def commands(conn_type, device):
    ''' Get the list of commands for a specified device '''
    from .koheron import KoheronClient
    client = KoheronClient(host=board_host(conn_type))
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
    upload_instrument(board_host(conn_type), instrument_zip, run=run)

@cli.command()
@click.pass_obj
@click.argument('instrument_name', required=False)
@click.option('--restart', is_flag=True)
def run(conn_type, instrument_name, restart):
    ''' Run a given instrument '''
    from .koheron import run_instrument
    run_instrument(board_host(conn_type), instrument_name, restart=restart)


@cli.command()
@click.pass_obj
@click.argument('archive', type=click.Path(exists=True, dir_okay=False))
@click.option('--name')
@click.option('--timeout', type=float, default=120, show_default=True)
@click.option('--format', type=click.Choice(['text', 'json']), default='text')
def deploy(conn_type, archive, name, timeout, format):
    '''Upload, activate, confirm readiness, then return.'''
    from .management import execute
    raise SystemExit(execute('deploy', board_host(conn_type), timeout, format, archive=archive, name=name))


@cli.command()
@click.pass_obj
@click.option('--timeout', type=float, default=10, show_default=True)
@click.option('--format', type=click.Choice(['text', 'json']), default='text')
def status(conn_type, timeout, format):
    '''Read board and instrument status.'''
    from .management import execute
    raise SystemExit(execute('status', board_host(conn_type), timeout, format))


@cli.command()
@click.pass_obj
@click.option('--limit', type=click.IntRange(1, 2000), default=100, show_default=True)
@click.option('--follow', is_flag=True)
@click.option('--timeout', type=float, default=10, show_default=True)
@click.option('--format', type=click.Choice(['text', 'json']), default='text')
def logs(conn_type, limit, follow, timeout, format):
    '''Read recent server logs; --follow streams until Ctrl+C.'''
    from .management import execute
    raise SystemExit(execute('logs', board_host(conn_type), timeout, format, limit=limit, follow=follow))


if __name__ == '__main__':
    cli()
