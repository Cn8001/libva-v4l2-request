/*
 * Copyright (C) 2016 Florent Revest <florent.revest@free-electrons.com>
 * Copyright (C) 2018 Paul Kocialkowski <paul.kocialkowski@bootlin.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "mpeg2.h"
#include "context.h"
#include "request.h"
#include "surface.h"

#include <assert.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

#include "v4l2.h"

int mpeg2_set_controls(struct request_data *driver_data,
		       struct object_context *context_object,
		       struct object_surface *surface_object)
{
	VAPictureParameterBufferMPEG2 *picture =
		&surface_object->params.mpeg2.picture;
	VAIQMatrixBufferMPEG2 *iqmatrix =
		&surface_object->params.mpeg2.iqmatrix;
	bool iqmatrix_set = surface_object->params.mpeg2.iqmatrix_set;
	struct v4l2_ctrl_mpeg2_sequence sequence;
	struct v4l2_ctrl_mpeg2_picture pic;
	struct v4l2_ctrl_mpeg2_quantisation quantisation;
	struct object_surface *forward_reference_surface;
	struct object_surface *backward_reference_surface;
	uint64_t timestamp;
	unsigned int i;
	int rc;

	memset(&sequence, 0, sizeof(sequence));

	sequence.horizontal_size = picture->horizontal_size;
	sequence.vertical_size = picture->vertical_size;
	sequence.vbv_buffer_size = SOURCE_SIZE_MAX;
	sequence.profile_and_level_indication = 0;
	sequence.chroma_format = 1; // 4:2:0

	memset(&pic, 0, sizeof(pic));

	pic.picture_coding_type = picture->picture_coding_type;
	pic.f_code[0][0] = (picture->f_code >> 12) & 0x0f;
	pic.f_code[0][1] = (picture->f_code >> 8) & 0x0f;
	pic.f_code[1][0] = (picture->f_code >> 4) & 0x0f;
	pic.f_code[1][1] = (picture->f_code >> 0) & 0x0f;

	pic.intra_dc_precision =
		picture->picture_coding_extension.bits.intra_dc_precision;
	pic.picture_structure =
		picture->picture_coding_extension.bits.picture_structure;

	if (picture->picture_coding_extension.bits.top_field_first)
		pic.flags |= V4L2_MPEG2_PIC_FLAG_TOP_FIELD_FIRST;
	if (picture->picture_coding_extension.bits.frame_pred_frame_dct)
		pic.flags |= V4L2_MPEG2_PIC_FLAG_FRAME_PRED_DCT;
	if (picture->picture_coding_extension.bits.concealment_motion_vectors)
		pic.flags |= V4L2_MPEG2_PIC_FLAG_CONCEALMENT_MV;
	if (picture->picture_coding_extension.bits.q_scale_type)
		pic.flags |= V4L2_MPEG2_PIC_FLAG_Q_SCALE_TYPE;
	if (picture->picture_coding_extension.bits.intra_vlc_format)
		pic.flags |= V4L2_MPEG2_PIC_FLAG_INTRA_VLC;
	if (picture->picture_coding_extension.bits.alternate_scan)
		pic.flags |= V4L2_MPEG2_PIC_FLAG_ALT_SCAN;
	if (picture->picture_coding_extension.bits.repeat_first_field)
		pic.flags |= V4L2_MPEG2_PIC_FLAG_REPEAT_FIRST;
	if (picture->picture_coding_extension.bits.progressive_frame)
		pic.flags |= V4L2_MPEG2_PIC_FLAG_PROGRESSIVE;

	forward_reference_surface =
		SURFACE(driver_data, picture->forward_reference_picture);
	if (forward_reference_surface == NULL)
		forward_reference_surface = surface_object;

	timestamp = v4l2_timeval_to_ns(&forward_reference_surface->timestamp);
	pic.forward_ref_ts = timestamp;

	backward_reference_surface =
		SURFACE(driver_data, picture->backward_reference_picture);
	if (backward_reference_surface == NULL)
		backward_reference_surface = surface_object;

	timestamp = v4l2_timeval_to_ns(&backward_reference_surface->timestamp);
	pic.backward_ref_ts = timestamp;

	rc = v4l2_set_control(driver_data->video_fd, surface_object->request_fd,
			      V4L2_CID_STATELESS_MPEG2_SEQUENCE,
			      &sequence, sizeof(sequence));
	if (rc < 0)
		return VA_STATUS_ERROR_OPERATION_FAILED;

	rc = v4l2_set_control(driver_data->video_fd, surface_object->request_fd,
			      V4L2_CID_STATELESS_MPEG2_PICTURE,
			      &pic, sizeof(pic));
	if (rc < 0)
		return VA_STATUS_ERROR_OPERATION_FAILED;

	if (iqmatrix_set) {
		memset(&quantisation, 0, sizeof(quantisation));

		for (i = 0; i < 64; i++) {
			quantisation.intra_quantiser_matrix[i] =
				iqmatrix->intra_quantiser_matrix[i];
			quantisation.non_intra_quantiser_matrix[i] =
				iqmatrix->non_intra_quantiser_matrix[i];
			quantisation.chroma_intra_quantiser_matrix[i] =
				iqmatrix->chroma_intra_quantiser_matrix[i];
			quantisation.chroma_non_intra_quantiser_matrix[i] =
				iqmatrix->chroma_non_intra_quantiser_matrix[i];
		}

		rc = v4l2_set_control(driver_data->video_fd,
				      surface_object->request_fd,
				      V4L2_CID_STATELESS_MPEG2_QUANTISATION,
				      &quantisation, sizeof(quantisation));
		if (rc < 0)
			return VA_STATUS_ERROR_OPERATION_FAILED;
	}

	return 0;
}
