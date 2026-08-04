/*
 * This file is part of libplacebo.
 *
 * libplacebo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libplacebo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libplacebo. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include <libplacebo/utils/dolbyvision.h>

#ifdef PL_HAVE_LIBDOVI
#include <libplacebo/tone_mapping.h>
#include <libdovi/rpu_parser.h>
#endif

void pl_hdr_metadata_from_dovi_rpu(struct pl_hdr_metadata *out,
                                   const uint8_t *buf, size_t size)
{
#ifdef PL_HAVE_LIBDOVI
    if (buf && size) {
        DoviRpuOpaque *rpu =
            dovi_parse_unspec62_nalu(buf, size);
        const DoviRpuDataHeader *header = dovi_rpu_get_header(rpu);

        if (header && header->vdr_dm_metadata_present_flag) {
            // Profile 4 reshaping isn't done as it is a dual layer format.
            // However there are still unknowns on its EOTF, so it cannot be enabled.
            //
            // For profile 7, the brightness metadata can still be used as most
            // titles are going to have accurate metadata<->image brightness,
            // with the exception of some titles that require the enhancement layer
            // to be processed to restore the intended brightness, which would then
            // match the metadata values.
            if (header->guessed_profile == 4) {
                goto done;
            }

            const DoviVdrDmData *vdr_dm_data = dovi_rpu_get_vdr_dm_data(rpu);
            if (vdr_dm_data->dm_data.level1) {
                int max_pq_offset = 0;
                int avg_pq_offset = 0;
                int min_pq_offset = 0;

                if (vdr_dm_data->dm_data.level3) {
                    const DoviExtMetadataBlockLevel3 *l3 = vdr_dm_data->dm_data.level3;
                    max_pq_offset = l3->max_pq_offset - 2048;
                    avg_pq_offset = l3->avg_pq_offset - 2048;
                    min_pq_offset = l3->min_pq_offset - 2048;
                }

                const DoviExtMetadataBlockLevel1 *l1 = vdr_dm_data->dm_data.level1;
                out->dovi_max_pq = (l1->max_pq + max_pq_offset) / 4095.0f;
                out->dovi_avg_pq = (l1->avg_pq + avg_pq_offset) / 4095.0f;
                out->dovi_min_pq = (l1->min_pq + min_pq_offset) / 4095.0f;
            }

            if (vdr_dm_data->dm_data.level2.len) {
                out->num_dovi_trims = PL_MIN(vdr_dm_data->dm_data.level2.len, PL_ARRAY_SIZE(out->dovi_trims));
                for (int i = 0; i < out->num_dovi_trims; i++) {
                    const DoviExtMetadataBlockLevel2 *l2 = vdr_dm_data->dm_data.level2.list[i];
                    out->dovi_trims[i].target_max_pq = l2->target_max_pq / 4095.0f;
                    out->dovi_trims[i].trim_slope = l2->trim_slope / 4096.0f + 0.5f;
                    out->dovi_trims[i].trim_offset = l2->trim_offset / 4096.0f - 0.5f;
                    out->dovi_trims[i].trim_power = l2->trim_power / 4096.0f + 0.5f;
                    out->dovi_trims[i].trim_saturation_gain = l2->trim_saturation_gain / 4096.0f - 0.5f;
                    out->dovi_trims[i].trim_chroma_weight = l2->trim_chroma_weight / 4096.0f - 0.5f;
                }

                // Sort trims by target_max_pq
                for (int i = 0; i < out->num_dovi_trims - 1; i++) {
                    for (int j = i + 1; j < out->num_dovi_trims; j++) {
                        if (out->dovi_trims[i].target_max_pq > out->dovi_trims[j].target_max_pq) {
                            struct pl_hdr_dovi_trims tmp = out->dovi_trims[i];
                            out->dovi_trims[i] = out->dovi_trims[j];
                            out->dovi_trims[j] = tmp;
                        }
                    }
                }
            }

            dovi_rpu_free_vdr_dm_data(vdr_dm_data);
        }

    done:
        dovi_rpu_free_header(header);
        dovi_rpu_free(rpu);
    }
#endif
}
