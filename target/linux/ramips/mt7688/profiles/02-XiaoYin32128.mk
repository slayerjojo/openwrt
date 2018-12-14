#
# Copyright (C) 2015 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/XiaoYin32128
	NAME:=YinYun XiaoYin 32MB flash/128MB RAM
	PACKAGES:=\
	kmod-usb-core kmod-usb2 kmod-usb-ohci \
	kmod-ledtrig-netdev kmod-ledtrig-usbdev kmod-ledtrig-gpio kmod-ledtrig-heartbeat \
	mountd \
	uhttpd rpcd rpcd-mod-iwinfo \
	kmod-fs-vfat kmod-fs-exfat kmod-fs-ext4 block-mount e2fsprogs \
	kmod-nls-base kmod-nls-cp437 kmod-nls-iso8859-1 kmod-nls-utf8 kmod-usb-storage \
	mtk-wifi airkiss webui ated luci reg maccalc \
	gateway luci-i18n-base-zh-cn \
	mtk-mt76x8-linkwatch
endef


define Profile/XiaoYin32128/Config
	select LUCI_LANG_zh-cn
endef


define Profile/XiaoYin32127/Description
	YinYun 32MB flash/128MB ram base packages.
endef

$(eval $(call Profile,XiaoYin32128))
