#include "AmeriProfile.h"
#include <esp_app_desc.h>
#include <esp_idf_version.h>
#include <sdkconfig.h>
#define AMERI_STR_INNER(x) #x
#define AMERI_STR(x) AMERI_STR_INNER(x)
// ESP-IDF 5.x supplies a weak descriptor and native accessors/startup handling.
// Override only the descriptor; retain native ELF-hash initialization/accessors.
extern "C" const esp_app_desc_t esp_app_desc __attribute__((section(".rodata_desc"),used))={
 .magic_word=ESP_APP_DESC_MAGIC_WORD,
#ifdef CONFIG_BOOTLOADER_APP_SECURE_VERSION
 .secure_version=CONFIG_BOOTLOADER_APP_SECURE_VERSION,
#else
 .secure_version=0,
#endif
 .reserv1={0,0},
 .version=AMERI_VERSION,
 .project_name=AMERI_PROJECT,
 .time=__TIME__,
 .date=__DATE__,
 .idf_ver="v" AMERI_STR(ESP_IDF_VERSION_MAJOR) "." AMERI_STR(ESP_IDF_VERSION_MINOR) "." AMERI_STR(ESP_IDF_VERSION_PATCH),
 .app_elf_sha256={0},
 .min_efuse_blk_rev_full=CONFIG_ESP_EFUSE_BLOCK_REV_MIN_FULL,
 .max_efuse_blk_rev_full=CONFIG_ESP_EFUSE_BLOCK_REV_MAX_FULL,
 .mmu_page_size=31-__builtin_clz(CONFIG_MMU_PAGE_SIZE),
 .reserv3={0},
 .reserv2={0}
};
// Preserve deferred confirmation until the application self-test completes.
extern "C" bool verifyRollbackLater(){return true;}
