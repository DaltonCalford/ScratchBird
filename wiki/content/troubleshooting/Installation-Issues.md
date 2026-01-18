# Installation Issues

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

## AppImage Will Not Run
- Ensure FUSE is available: `sudo modprobe fuse`
- Make the file executable: `chmod +x scratchbird.AppImage`

## DEB/RPM Dependency Errors
- Re-run dependency resolution:
  - Debian/Ubuntu: `sudo apt-get -f install`
  - RHEL/Fedora: `sudo dnf install ./scratchbird.rpm`

## Service Fails to Start
- Check logs in `/var/log/scratchbird/` (if packaged)
- Run `sb_server` in the foreground to see errors

See:
- `../installation/DEB-Package.md`
- `../installation/RPM-Package.md`
- `../installation/AppImage.md`
