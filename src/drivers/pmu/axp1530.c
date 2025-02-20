/* SPDX-License-Identifier: Apache-2.0 */

#include <io.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <types.h>

#include <common.h>
#include <log.h>

#include <pmu/axp.h>

/* clang-format off */
static axp_contrl_info axp_ctrl_tbl[] = {
	{ "dcdc1", 500, 3400, AXP1530_DC1OUT_VOL, 0x7f, AXP1530_OUTPUT_POWER_ON_OFF_CTL, 0, 0,
	{ {500, 1200, 10}, {1220, 1540, 20}, {1600, 3400, 100}, } },

	{ "dcdc2", 500, 1540, AXP1530_DC2OUT_VOL, 0x7f, AXP1530_OUTPUT_POWER_ON_OFF_CTL, 1, 0,
	{ {500, 1200, 10}, {1220, 1540, 20}, } },

	{ "dcdc3", 500, 1840, AXP1530_DC3OUT_VOL, 0x7f, AXP1530_OUTPUT_POWER_ON_OFF_CTL, 2, 0,
	{ {500, 1200, 10}, {1220, 1840, 20}, } },

	{ "aldo1", 500, 3500, AXP1530_ALDO1OUT_VOL, 0x1f, AXP1530_OUTPUT_POWER_ON_OFF_CTL, 3, 0,
	{ {500, 3500, 100}, } },

	{ "dldo1", 500, 3500, AXP1530_DLDO1OUT_VOL, 0x1f, AXP1530_OUTPUT_POWER_ON_OFF_CTL, 4, 0,
	{ {500, 3500, 100}, } },
};
/* clang-format on */

/**
 * Get control information from the table based on the given name.
 *
 * @param name The name of the control information to retrieve.
 * @return A pointer to the axp_contrl_info structure corresponding to the given name,
 *         or NULL if the name is not found in the table.
 */
static axp_contrl_info *get_ctrl_info_from_tbl(char *name) {
    int i = 0;
    int size = ARRAY_SIZE(axp_ctrl_tbl);
    for (i = 0; i < size; i++) {
        if (!strncmp(name, axp_ctrl_tbl[i].name, strlen(axp_ctrl_tbl[i].name))) {
            break;
        }
    }
    if (i >= size) {
        return NULL;
    }
    return (axp_ctrl_tbl + i);
}

int pmu_axp1530_init(sunxi_i2c_t *i2c_dev) {
    uint8_t axp_val;
    int ret;

    if (!i2c_dev->status) {
        printk(LOG_LEVEL_WARNING, "PMU: I2C not init\n");
        return -1;
    }

    if (ret = sunxi_i2c_read(i2c_dev, AXP1530_RUNTIME_ADDR, AXP1530_VERSION, &axp_val)) {
        printk(LOG_LEVEL_WARNING, "PMU: Probe target device AXP1530 failed. ret = %d\n", ret);
        return -1;
    }

    axp_val &= 0xCF;
    switch (axp_val) {
        case AXP1530_CHIP_ID:
            printk(LOG_LEVEL_INFO, "PMU: Found AXP1530 PMU\n");
            break;
        case AXP313A_CHIP_ID:
            printk(LOG_LEVEL_INFO, "PMU: Found AXP313A PMU\n");
            break;
        case AXP313B_CHIP_ID:
            printk(LOG_LEVEL_INFO, "PMU: Found AXP313B PMU\n");
            break;
        case AXP323_CHIP_ID:
            printk(LOG_LEVEL_INFO, "PMU: Found AXP323 PMU\n");
            break;
        default:
            printk(LOG_LEVEL_INFO, "PMU: Cannot found match PMU\n");
            return -1;
    }

    /* Set over temperature shutdown functtion */
    if (sunxi_i2c_read(i2c_dev, AXP1530_RUNTIME_ADDR, AXP1530_POWER_DOMN_SEQUENCE, &axp_val))
        return -1;
    axp_val |= (0x1 << 1);
    if (sunxi_i2c_write(i2c_dev, AXP1530_RUNTIME_ADDR, AXP1530_POWER_DOMN_SEQUENCE, axp_val))
        return -1;

    return 0;
}

int pmu_axp1530_set_dual_phase(sunxi_i2c_t *i2c_dev) {
    uint8_t axp_val;
    int ret;

    if (ret = sunxi_i2c_read(i2c_dev, AXP1530_RUNTIME_ADDR, AXP1530_VERSION, &axp_val)) {
        printk(LOG_LEVEL_WARNING, "PMU: Probe target device AXP1530 failed. ret = %d\n", ret);
        return -1;
    }

    axp_val &= 0xCF;
    switch (axp_val) {
        case AXP323_CHIP_ID: /* Only AXP323 Support Dual phase */
            break;
        default:
            printk(LOG_LEVEL_INFO, "PMU: PMU not support dual phase\n");
            return -1;
    }

    sunxi_i2c_write(i2c_dev, AXP1530_RUNTIME_ADDR, AXP1530_OUTPUT_MONITOR_CONTROL, 0x1E);
    sunxi_i2c_write(i2c_dev, AXP1530_RUNTIME_ADDR, AXP1530_DCDC_MODE_CTRL2, 0x02);
    sunxi_i2c_write(i2c_dev, AXP1530_RUNTIME_ADDR, AXP1530_POWER_DOMN_SEQUENCE, 0x22);

    return 0;
}

int pmu_axp1530_set_vol(sunxi_i2c_t *i2c_dev, char *name, int set_vol, int onoff) {
    uint8_t reg_value, i;
    axp_contrl_info *p_item = NULL;
    uint8_t base_step = 0;

    p_item = get_ctrl_info_from_tbl(name);
    if (!p_item) {
        return -1;
    }

    if ((set_vol > 0) && (p_item->min_vol)) {
        if (set_vol < p_item->min_vol) {
            set_vol = p_item->min_vol;
        } else if (set_vol > p_item->max_vol) {
            set_vol = p_item->max_vol;
        }

        if (sunxi_i2c_read(i2c_dev, AXP1530_RUNTIME_ADDR, p_item->cfg_reg_addr, &reg_value)) {
            return -1;
        }

        reg_value &= ~p_item->cfg_reg_mask;

        for (i = 0; p_item->axp_step_tbl[i].step_max_vol != 0; i++) {
            if ((set_vol > p_item->axp_step_tbl[i].step_max_vol) && (set_vol < p_item->axp_step_tbl[i + 1].step_min_vol)) {
                set_vol = p_item->axp_step_tbl[i].step_max_vol;
            }
            if (p_item->axp_step_tbl[i].step_max_vol >= set_vol) {
                reg_value |= ((base_step + ((set_vol - p_item->axp_step_tbl[i].step_min_vol) / p_item->axp_step_tbl[i].step_val)) << p_item->reg_addr_offset);
                if (p_item->axp_step_tbl[i].regation) {
                    u8 reg_value_temp = (~reg_value & p_item->cfg_reg_mask);
                    reg_value &= ~p_item->cfg_reg_mask;
                    reg_value |= reg_value_temp;
                }
                break;
            } else {
                base_step += ((p_item->axp_step_tbl[i].step_max_vol - p_item->axp_step_tbl[i].step_min_vol + p_item->axp_step_tbl[i].step_val) / p_item->axp_step_tbl[i].step_val);
            }
        }

        if (sunxi_i2c_write(i2c_dev, AXP1530_RUNTIME_ADDR, p_item->cfg_reg_addr, reg_value)) {
            return -1;
        }
    }

    if (onoff < 0) {
        return 0;
    }
    if (sunxi_i2c_read(i2c_dev, AXP1530_RUNTIME_ADDR, p_item->ctrl_reg_addr, &reg_value)) {
        return -1;
    }
    if (onoff == 0) {
        reg_value &= ~(1 << p_item->ctrl_bit_ofs);
    } else {
        reg_value |= (1 << p_item->ctrl_bit_ofs);
    }
    if (sunxi_i2c_write(i2c_dev, AXP1530_RUNTIME_ADDR, p_item->ctrl_reg_addr, reg_value)) {
        return -1;
    }
    return 0;
}

int pmu_axp1530_get_vol(sunxi_i2c_t *i2c_dev, char *name) {
    uint8_t reg_value, i;
    axp_contrl_info *p_item = NULL;
    uint8_t base_step1 = 0;
    uint8_t base_step2 = 0;
    int vol;

    p_item = get_ctrl_info_from_tbl(name);
    if (!p_item) {
        return -1;
    }

    if (sunxi_i2c_read(i2c_dev, AXP1530_RUNTIME_ADDR, p_item->ctrl_reg_addr, &reg_value)) {
        return -1;
    }

    if (!(reg_value & (0x01 << p_item->ctrl_bit_ofs))) {
        return 0;
    }

    if (sunxi_i2c_read(i2c_dev, AXP1530_RUNTIME_ADDR, p_item->cfg_reg_addr, &reg_value)) {
        return -1;
    }
    reg_value &= p_item->cfg_reg_mask;
    reg_value >>= p_item->reg_addr_offset;
    for (i = 0; p_item->axp_step_tbl[i].step_max_vol != 0; i++) {
        base_step1 += ((p_item->axp_step_tbl[i].step_max_vol - p_item->axp_step_tbl[i].step_min_vol + p_item->axp_step_tbl[i].step_val) / p_item->axp_step_tbl[i].step_val);
        if (reg_value < base_step1) {
            vol = (reg_value - base_step2) * p_item->axp_step_tbl[i].step_val + p_item->axp_step_tbl[i].step_min_vol;
            return vol;
        }
        base_step2 += ((p_item->axp_step_tbl[i].step_max_vol - p_item->axp_step_tbl[i].step_min_vol + p_item->axp_step_tbl[i].step_val) / p_item->axp_step_tbl[i].step_val);
    }
    return -1;
}

void pmu_axp1530_dump(sunxi_i2c_t *i2c_dev) {
    printk(LOG_LEVEL_DEBUG, "PMU: AXP1530 DCDC1 = %dmv\n", pmu_axp1530_get_vol(i2c_dev, "dcdc1"));
    printk(LOG_LEVEL_DEBUG, "PMU: AXP1530 DCDC2 = %dmv\n", pmu_axp1530_get_vol(i2c_dev, "dcdc2"));
    printk(LOG_LEVEL_DEBUG, "PMU: AXP1530 DCDC3 = %dmv\n", pmu_axp1530_get_vol(i2c_dev, "dcdc3"));
    printk(LOG_LEVEL_DEBUG, "PMU: AXP1530 ALDO1 = %dmv\n", pmu_axp1530_get_vol(i2c_dev, "aldo1"));
    printk(LOG_LEVEL_DEBUG, "PMU: AXP1530 DLDO1 = %dmv\n", pmu_axp1530_get_vol(i2c_dev, "dldo1"));
}