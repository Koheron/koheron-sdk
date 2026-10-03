# OS image builds

For build and installation commands, see the [SDK quick start](../README.md#quick-start).

## Image settings

Set `PASSWORD` in the environment before `make image` to customize the image's root password; `PASSWD` is accepted as a legacy alias. Changes to `PASSWORD` or `TIMEZONE` rebuild the cached base rootfs. The default password is `changeme` and the default timezone is `Europe/Paris`.

## Build behavior

Each board generates its own machine ID and SSH host keys on first boot. Build-only QEMU helpers, chroot scripts and the temporary dpkg `force-unsafe-io` setting are removed before packaging. Base-rootfs builds replace the cached tarball only after a successful build and archive operation.

Base-rootfs builds stop if any APT repository refresh fails after the configured retries. Run the isolated APT tests with `python3 os/tests/test_rootfs_apt_update.py`; they use private APT state and a local HTTP repository without installing packages.

Make verifies the Ubuntu source tarball against the release checksums before reusing it. Downloads replace cached inputs only after successful validation; failed or interrupted downloads leave the previous files intact. A valid cached source can be reused offline without rebuilding the configured base rootfs. Run the download regression tests with `python3 os/tests/test_rootfs_download_make.py`.

Image builds check required input files before overwriting the loose image or allocating a loop device. They verify the resized partition before truncating and release chroot mounts and the loop device before packaging. If a mount cannot be released, the build fails and reports the loop device retained for recovery.

Overlay configuration fails if a required service cannot be enabled, so the build cannot publish a ZIP with incomplete service setup. Run the isolated chroot tests with `docker run --rm -v "$PWD":/sdk:ro -w /sdk cross-armhf:24.04 python3 os/tests/test_rootfs_overlay.py`; these tests do not mount disks or start services.

Each ZIP is built from scratch and atomically replaces the previous archive after successful packaging; failed builds preserve the previous ZIP. The loose image and checksum files are build outputs and may change during a failed rebuild. Run the disk-free build regression tests with `python3 -m unittest discover -s os/tests -p 'test_*build.py'`.
