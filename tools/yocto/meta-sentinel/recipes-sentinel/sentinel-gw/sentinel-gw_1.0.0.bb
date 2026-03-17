SUMMARY = "Sentinel Gateway Application"
DESCRIPTION = "Linux gateway daemon for the Sentinel Gateway dual-processor system"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://sentinel-gw.service"

DEPENDS = "protobuf-c"
RDEPENDS:${PN} = "protobuf-c"

inherit cmake systemd

SYSTEMD_SERVICE:${PN} = "sentinel-gw.service"
SYSTEMD_AUTO_ENABLE = "enable"

# Install systemd service
do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/sentinel-gw.service ${D}${systemd_system_unitdir}/

    # Create log directory
    install -d ${D}/var/log/sentinel
}

FILES:${PN} += "${systemd_system_unitdir}/sentinel-gw.service /var/log/sentinel"
