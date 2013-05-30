USE_CAMERA_STUB := true

# inherit from the proprietary version
-include vendor/zte/n980/BoardConfigVendor.mk

TARGET_ARCH := arm
TARGET_NO_BOOTLOADER := true
TARGET_BOARD_PLATFORM := unknown
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_ARCH_VARIANT := armv7-a-neon
ARCH_ARM_HAVE_TLS_REGISTER := true

TARGET_BOOTLOADER_BOARD_NAME := n980

BOARD_KERNEL_CMDLINE := console=ttyHSL0,115200,n8 androidboot.hardware=qcom vmalloc=200M
BOARD_KERNEL_BASE := 0x00200000
BOARD_KERNEL_PAGESIZE := 4096

# fix this up by examining /proc/mtd on a running device
BOARD_BOOTIMAGE_PARTITION_SIZE := 0x105c0000
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 0x105c0000
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 0x105c0000
BOARD_USERDATAIMAGE_PARTITION_SIZE := 0x105c0000
BOARD_FLASH_BLOCK_SIZE := 131072

TARGET_PREBUILT_KERNEL := device/zte/n980/kernel

BOARD_HAS_NO_SELECT_BUTTON := true

#TARGET_SPECIFIC_HEADER_PATH := device/zte/n980/include

BOARD_CUSTOM_RECOVERY_KEYMAPPING := ../../device/zte/n980/recovery/recovery_keys.c
BOARD_CUSTOM_GRAPHICS            := ../../../device/zte/n980/recovery/graphics.c
BOARD_UMS_LUNFILE                := "/sys/devices/platform/msm_hsusb/gadget/lun%d/file"
#BOARD_UMS_LUNFILE := "/sys/class/android_usb/android0/f_mass_storage/lun%d/file"
#BOARD_UMS_2ND_LUNFILE := "/sys/class/android_usb/android0/f_mass_storage/lun1/file"
TARGET_RECOVERY_INITRC		 := device/zte/n980/recovery/init.rc
#TARGET_RECOVERY_PIXEL_FORMAT	:= "RGBX_8888"
#BOARD_USE_SCREENCAP := true

