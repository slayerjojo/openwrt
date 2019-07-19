include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=iotsigma_gateway
PKG_VERSION:=1.0

define Package/iotsigma_gateway
	CATEGORY:=UltraIot

	#DEPENDS:= 如果需要依赖其他软件包，在这里添加

	TITLE:=iotsigma_gateway for test
	MAINTAINER:=chenjing <slayerjojo@163.com>
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/iotsigma_gateway/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/iotsigma_gateway $(1)/bin
endef

define Package/iotsigma_gateway/preinst
	#!/bin/sh
	echo 'before install $(PKG_NAME)'
endef

$(eval $(call BuildPackage,iotsigma_gateway))
