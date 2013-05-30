## Specify phone tech before including full_phone
$(call inherit-product, vendor/cm/config/gsm.mk)

# Release name
PRODUCT_RELEASE_NAME := n980

# Inherit some common CM stuff.
$(call inherit-product, vendor/cm/config/common_full_phone.mk)

# Inherit device configuration
$(call inherit-product, device/zte/n980/device_n980.mk)

## Device identifier. This must come after all inclusions
PRODUCT_DEVICE := n980
PRODUCT_NAME := cm_n980
PRODUCT_BRAND := zte
PRODUCT_MODEL := n980
PRODUCT_MANUFACTURER := zte
