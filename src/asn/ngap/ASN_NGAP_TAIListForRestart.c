/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "NGAP-IEs"
 * 	found in "NGAP-IEs.asn"
 * 	`asn1c -pdu=all -fcompound-names -findirect-choice -fno-include-deps -no-gen-OER -gen-PER -no-gen-example -D ngap`
 */

#include "ASN_NGAP_TAIListForRestart.h"

#include "ASN_NGAP_TAI.h"
asn_per_constraints_t asn_PER_type_ASN_NGAP_TAIListForRestart_constr_1 CC_NOTUSED = {
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	{ APC_CONSTRAINED,	 11,  11,  1,  2048 }	/* (SIZE(1..2048)) */,
	0, 0	/* No PER value map */
};
asn_TYPE_member_t asn_MBR_ASN_NGAP_TAIListForRestart_1[] = {
	{ ATF_POINTER, 0, 0,
		(ASN_TAG_CLASS_UNIVERSAL | (16 << 2)),
		0,
		&asn_DEF_ASN_NGAP_TAI,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		""
		},
};
static const ber_tlv_tag_t asn_DEF_ASN_NGAP_TAIListForRestart_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
asn_SET_OF_specifics_t asn_SPC_ASN_NGAP_TAIListForRestart_specs_1 = {
	sizeof(struct ASN_NGAP_TAIListForRestart),
	offsetof(struct ASN_NGAP_TAIListForRestart, _asn_ctx),
	0,	/* XER encoding is XMLDelimitedItemList */
};
asn_TYPE_descriptor_t asn_DEF_ASN_NGAP_TAIListForRestart = {
	"TAIListForRestart",
	"TAIListForRestart",
	&asn_OP_SEQUENCE_OF,
	asn_DEF_ASN_NGAP_TAIListForRestart_tags_1,
	sizeof(asn_DEF_ASN_NGAP_TAIListForRestart_tags_1)
		/sizeof(asn_DEF_ASN_NGAP_TAIListForRestart_tags_1[0]), /* 1 */
	asn_DEF_ASN_NGAP_TAIListForRestart_tags_1,	/* Same as above */
	sizeof(asn_DEF_ASN_NGAP_TAIListForRestart_tags_1)
		/sizeof(asn_DEF_ASN_NGAP_TAIListForRestart_tags_1[0]), /* 1 */
	{ 0, &asn_PER_type_ASN_NGAP_TAIListForRestart_constr_1, SEQUENCE_OF_constraint },
	asn_MBR_ASN_NGAP_TAIListForRestart_1,
	1,	/* Single element */
	&asn_SPC_ASN_NGAP_TAIListForRestart_specs_1	/* Additional specs */
};

