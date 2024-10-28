/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "NGAP-IEs"
 * 	found in "NGAP-IEs.asn"
 * 	`asn1c -pdu=all -fcompound-names -findirect-choice -fno-include-deps -no-gen-OER -gen-PER -no-gen-example -D ngap`
 */

#include "ASN_NGAP_RAT-Information.h"

/*
 * This type is implemented using NativeEnumerated,
 * so here we adjust the DEF accordingly.
 */
asn_per_constraints_t asn_PER_type_ASN_NGAP_RAT_Information_constr_1 CC_NOTUSED = {
	{ APC_CONSTRAINED | APC_EXTENSIBLE,  1,  1,  0,  1 }	/* (0..1,...) */,
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	0, 0	/* No PER value map */
};
static const asn_INTEGER_enum_map_t asn_MAP_ASN_NGAP_RAT_Information_value2enum_1[] = {
	{ 0,	10,	"unlicensed" },
	{ 1,	6,	"nb-IoT" },
	{ 2,	6,	"nR-LEO" },
	{ 3,	6,	"nR-MEO" },
	{ 4,	6,	"nR-GEO" },
	{ 5,	11,	"nR-OTHERSAT" }
	/* This list is extensible */
};
static const unsigned int asn_MAP_ASN_NGAP_RAT_Information_enum2value_1[] = {
	4,	/* nR-GEO(4) */
	2,	/* nR-LEO(2) */
	3,	/* nR-MEO(3) */
	5,	/* nR-OTHERSAT(5) */
	1,	/* nb-IoT(1) */
	0	/* unlicensed(0) */
	/* This list is extensible */
};
const asn_INTEGER_specifics_t asn_SPC_ASN_NGAP_RAT_Information_specs_1 = {
	asn_MAP_ASN_NGAP_RAT_Information_value2enum_1,	/* "tag" => N; sorted by tag */
	asn_MAP_ASN_NGAP_RAT_Information_enum2value_1,	/* N => "tag"; sorted by N */
	6,	/* Number of elements in the maps */
	3,	/* Extensions before this member */
	1,	/* Strict enumeration */
	0,	/* Native long size */
	0
};
static const ber_tlv_tag_t asn_DEF_ASN_NGAP_RAT_Information_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (10 << 2))
};
asn_TYPE_descriptor_t asn_DEF_ASN_NGAP_RAT_Information = {
	"RAT-Information",
	"RAT-Information",
	&asn_OP_NativeEnumerated,
	asn_DEF_ASN_NGAP_RAT_Information_tags_1,
	sizeof(asn_DEF_ASN_NGAP_RAT_Information_tags_1)
		/sizeof(asn_DEF_ASN_NGAP_RAT_Information_tags_1[0]), /* 1 */
	asn_DEF_ASN_NGAP_RAT_Information_tags_1,	/* Same as above */
	sizeof(asn_DEF_ASN_NGAP_RAT_Information_tags_1)
		/sizeof(asn_DEF_ASN_NGAP_RAT_Information_tags_1[0]), /* 1 */
	{ 0, &asn_PER_type_ASN_NGAP_RAT_Information_constr_1, NativeEnumerated_constraint },
	0, 0,	/* Defined elsewhere */
	&asn_SPC_ASN_NGAP_RAT_Information_specs_1	/* Additional specs */
};

