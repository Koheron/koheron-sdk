import click

# --------------------------------------------
# Call koheron-server
# --------------------------------------------

def board_host(host):
    if not host:
        raise click.UsageError('Specify --host or set HOST')
    return host

@click.group()
@click.option('--host', default='', help='Board IP or hostname', envvar='HOST')
@click.pass_context
def cli(ctx, host):
    ctx.obj = host

@cli.command()
def version():
    ''' Get the version of koheron python library '''
    from .version import __version__
    click.echo(__version__)

@cli.command()
@click.pass_obj
def devices(host):
    ''' Get the list of devices '''
    from .koheron import KoheronClient
    client = KoheronClient(host=board_host(host))
    click.echo(client.devices_idx)

@cli.command()
@click.pass_obj
@click.option('--device', default=None)
def commands(host, device):
    ''' Get the list of commands for a specified device '''
    from .koheron import KoheronClient
    client = KoheronClient(host=board_host(host))
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
def upload(host, instrument_zip, run):
    ''' Upload instrument.zip '''
    from .koheron import upload_instrument
    upload_instrument(board_host(host), instrument_zip, run=run)

@cli.command()
@click.pass_obj
@click.argument('instrument_name', required=False)
@click.option('--restart', is_flag=True)
def run(host, instrument_name, restart):
    ''' Run a given instrument '''
    from .koheron import run_instrument
    run_instrument(board_host(host), instrument_name, restart=restart)


if __name__ == '__main__':
    cli()
