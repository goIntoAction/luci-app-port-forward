include $(TOPDIR)/rules.mk

# Define the package name
PKG_NAME:=port_forward
PKG_VERSION:=1.0
PKG_RELEASE:=1

# Source files
PKG_SOURCE_DIR := src
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

# Include package rules
include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
    SECTION:=net
    CATEGORY:=Network
    TITLE:=IPv6 to IPv4 Port Forwarding Daemon
    DEPENDS:=+libpthread +libubus +libubox # Add any dependencies if required
endef

define Package/$(PKG_NAME)/description
 This package forwards IPv6 traffic to specified IPv4 addresses and ports.
endef

define Build/Compile
    $(TARGET_CC) $(TARGET_CFLAGS) -o $(PKG_BUILD_DIR)/port_forward $(PKG_SOURCE_DIR)/main.c -lpthread
endef

define Package/$(PKG_NAME)/install
        $(INSTALL_DIR) $(1)/usr/bin
        $(INSTALL_BIN) $(PKG_BUILD_DIR)/port_forward $(1)/usr/bin/
        $(INSTALL_DIR) $(1)/etc/init.d
        # 生成init脚本
        @echo '#!/bin/sh' > $(1)/etc/init.d/port_forward
        @echo 'START=99' >> $(1)/etc/init.d/port_forward
        @echo 'STOP=10' >> $(1)/etc/init.d/port_forward
        @echo 'start() {' >> $(1)/etc/init.d/port_forward
        @echo '    echo "Starting port forward..."' >> $(1)/etc/init.d/port_forward
        @echo '    /usr/bin/port_forward &' >> $(1)/etc/init.d/port_forward
        @echo '}' >> $(1)/etc/init.d/port_forward
        @echo 'stop() {' >> $(1)/etc/init.d/port_forward
        @echo '    echo "Stopping port forward..."' >> $(1)/etc/init.d/port_forward
        @echo '    killall port_forward' >> $(1)/etc/init.d/port_forward
        @echo '}' >> $(1)/etc/init.d/port_forward
        # 设置权限
        chmod +x $(1)/etc/init.d/port_forward
        # 启用服务
        $(1)/etc/init.d/port_forward enable
endef

$(eval $(call BuildPackage,$(PKG_NAME)))