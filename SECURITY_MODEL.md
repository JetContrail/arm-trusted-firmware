# Threat model: JetContrail/arm-trusted-firmware

Trusted Firmware-A **v2.4** mirror (HEAD `e2c509a`, Nov 2020). Secure-world boot firmware (BL1/BL2) and EL3 runtime monitor (BL31) for Armv8-A/Armv7-A. This model was drafted from the code and the owner's answers, and is the threat model used to prioritise Devin security-scan findings. Upstream guidance: `docs/process/security.rst`, `docs/process/security-hardening.rst`, `docs/security_advisories/` (TFV-1..8).

## Owner's answers (authoritative)
- **Shipped platform: QEMU** (`plat/qemu`, virt and sbsa). Other platform ports are not built or shipped; findings reachable only through them are deprioritized (not dismissed).
- **Attackers: all of them matter**: compromised normal world, compromised secure payload, physical/boot-media attacker, hardware/side-channel attacker.
- **Out of scope / accepted risk: nothing.** The whole repository is in scope.
- Scope of the scan: the whole repository.
- No infrastructure repository (firmware only).

## System overview and trust boundaries
1. **Normal world → EL3** (strongest boundary). NS-EL1/EL2 (Linux, UEFI/EDK2, hypervisor) issues SMCs that BL31 dispatches through `bl31/aarch64/runtime_exceptions.S` and `common/runtime_svc.c` to:
   - Standard services: PSCI (`lib/psci`), SDEI (`services/std_svc/sdei`)
   - Arch services: SMCCC_ARCH_WORKAROUND_* (`services/arm_arch_svc`)
   - PMF and debugfs (`lib/pmf`, `lib/debugfs`)
   - Secure-payload dispatchers (`services/spd/*`), SPMD/FF-A (`services/std_svc/spmd`) and SPM-MM (`services/std_svc/spm_mm`)
   - Vendor SiP services (`plat/*/..._sip_svc.c`)
   
   Every SMC argument (addresses, sizes, indices, MPIDRs, entrypoints) is attacker-controlled.
2. **Secure payload → EL3.** The S-EL1/S-EL2 payload (OP-TEE via `services/spd/opteed`, plus TSP, Trusty and TLK) returns to EL3 with values the dispatchers trust. A compromised payload must not get EL3 execution or reach the other world's memory beyond its own.
3. **Boot media → BL1/BL2.**
   - BL1 is the ROM. On QEMU it is passed with `-bios`; BL1 then loads BL2, and BL2 loads BL31, BL32 and BL33.
   - Images come from a FIP in flash (`plat/qemu/common/qemu_io_storage.c`, `PLAT_QEMU_FIP_BASE`) or via semihosting.
   - The firmware parses FIP headers (`drivers/io/io_fip.c`), GPT/MBR partitions (`drivers/partition`) and X.509 certificates/TBBR CoT (`drivers/auth`, mbedTLS).
   - It also parses device trees (`lib/libfdt`, `lib/fconf`, `common/fdt_wrappers.c`) and gzip images (`lib/zlib`).
   - BL1 also runs firmware update (`bl1/bl1_fwu.c`): FWU SMCs from the normal world copy and authenticate images.
4. **Hardware boundary.** Fault injection, speculative execution (Spectre/BHB, `lib/cpus` errata/workarounds), PMU timing leakage, and cache/TLB state across world switches (`lib/el3_runtime` context save/restore).

## Major components
| Component | Code it covers |
|---|---|
| **Boot chain & image auth** | `bl1/`, `bl2/`, `bl2u/`, `drivers/auth/`, `drivers/io/`, `drivers/partition/`, `drivers/measured_boot/`, `lib/fconf/`, `lib/libfdt/`, `lib/zlib/`, `common/` (image loading, fdt wrappers, `bl_common.c`) |
| **EL3 runtime & standard services** | `bl31/`, `bl32/sp_min/`, `lib/el3_runtime/`, `lib/psci/`, `lib/cpus/`, `lib/extensions/`, `lib/pmf/`, `lib/debugfs/`, `services/std_svc/` (std_svc_setup, sdei), `services/arm_arch_svc/`, `common/runtime_svc.c` |
| **Secure payload dispatch & SPM** | `services/spd/`, `services/std_svc/spmd/`, `services/std_svc/spm_mm/`, `lib/optee/`, `bl32/tsp/`, `plat/common/plat_spmd_manifest.c` |
| **QEMU platform** | `plat/qemu/` (the shipped platform) |
| **Other platform ports** | every other `plat/*` vendor directory and its SiP services, plus vendor-specific `drivers/<vendor>/` |
| **Shared drivers & libraries** | the rest of `drivers/` (GIC, console, MMC, UFS, MTD, delay timer, etc.), `lib/libc/`, `lib/xlat_tables*/`, `lib/semihosting/`, `lib/locks/`, `lib/compiler-rt/`, `include/` |
| **Host tools** | `tools/` (fiptool, cert_create, encrypt_fw, sptool): run on the build host, not on the device |

## Entry points and who can reach them
- **Normal world (any NS kernel/hypervisor code):**
  - All BL31 SMC handlers and BL1 FWU SMCs, while BL1 is resident
  - SDEI event registration/dispatch, and the debugfs/PMF SMCs when built in
- **Secure payload:** SPD/SPMD return paths, FF-A messages, SPM-MM `MM_COMMUNICATE` buffers, and the boot-time args it passes back.
- **Physical / boot-media attacker:** FIP and image contents, certificates, partition tables, DTBs (QEMU generates the DTB at run time and BL2 edits it), FWU images, and semihosted files.
- **Hardware attacker:** glitching around signature/hash checks, speculative side channels and PMU.

## Assets (most to least sensitive)
1. Integrity of EL3 code, data and translation tables (full system compromise).
2. Secure-world memory: BL32/OP-TEE, `SEC_DRAM`/`SEC_SRAM` (BL32 sits in secure DRAM on QEMU, `BL32_RAM_LOCATION := tdram`).
3. The chain of trust: ROTPK (`plat/qemu/common/qemu_rotpk.S`, `qemu_trusted_boot.c`), NV counters (anti-rollback), image hashes, decryption keys (`ENCRYPT_BL31/BL32`).
4. Availability of the secure world (the hardening guide says it must not crash under NS request floods).
5. Confidentiality of secure timing and state toward the normal world (PMU, registers left in context).

## Attacker profiles (all in scope per owner)
- **A1** compromised NS OS/hypervisor issuing arbitrary SMCs
- **A2** compromised secure payload
- **A3** physical attacker who controls flash/boot media or supplies a malicious FIP/update image
- **A4** hardware/side-channel attacker

## Existing controls
- TBBR chain of trust with mbedTLS (`TRUSTED_BOARD_BOOT`, `GENERATE_COT`), plus anti-rollback NV counters.
  - On QEMU, TBBR is **optional** (`plat/qemu/qemu/platform.mk`, `ifneq (${TRUSTED_BOARD_BOOT},0)`); without it, images are not authenticated.
- `SEPARATE_CODE_AND_RODATA := 1` on QEMU, and xlat tables mapping code RO/XN.
- Stack protector is available, but **QEMU defaults `ENABLE_STACK_PROTECTOR := 0`**.
- PSCI validates entrypoints through `plat_validate_ns_entrypoint`/`qemu_validate_ns_entrypoint`.
- Cache, errata and Spectre mitigations are in `lib/cpus` (`WORKAROUND_CVE_2017_5715`, `CVE_2018_3639`). MDCR_EL3/PMU hardening is per the security-hardening guide.

## Version-specific notes (for CVE/TFV tagging)
v2.4 predates later upstream advisories:
- **TFV-9**: Spectre-BHB, `SMCCC_ARCH_WORKAROUND_3`
- **TFV-10**: X.509 parser out-of-bounds read, CVE-2022-47630, `drivers/auth/mbedtls/mbedtls_x509_parser.c`
- **TFV-11** and later: CPU errata/FWU and SDEI fixes

Findings matching those should be tagged accordingly.

## Prioritisation rules derived from the owner's answers
- Findings reachable on the **QEMU platform** or in **platform-independent core code** (boot chain, EL3 runtime, SPD/SPM, shared drivers used by QEMU) keep the scanner's severity.
- Findings reachable **only** through a non-QEMU platform port (vendor SiP services, vendor drivers) are lowered one level, because the owner does not ship those platforms. They are never dismissed, since nothing is out of scope.
- No attacker class and no component is excluded, so nothing is lowered on attacker or surface grounds. Host tools stay in scope.

## Open questions
- Which QEMU build options are used: `TRUSTED_BOARD_BOOT`, `SPD=opteed`, `SPMD_SPM_AT_SEL2`, `ENABLE_STACK_PROTECTOR`, `DEBUG`? This determines whether auth/SPM findings are reachable.
- Is semihosting enabled in the deployed QEMU configuration (host file access from BL1/BL2)?
- Is this firmware used for production-like deployments or for research/emulation only?
