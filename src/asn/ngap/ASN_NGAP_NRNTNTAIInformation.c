/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "NGAP-IEs"
 * 	found in "NGAP-IEs.asn"
 * 	`asn1c -pdu=all -fcompound-names -findirect-choice -fno-include-deps -no-gen-OER -gen-PER -no-gen-example -D ngap`
 */

#include "ASN_NGAP_NRNTNTAIInformation.h"

#include "ASN_NGAP_ProtocolExtensionContainer.h"
asn_TYPE_member_t asn_MBR_ASN_NGAP_NRNTNTAIInformation_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct ASN_NGAP_NRNTNTAIInformation, servingPLMN),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_PLMNIdentity,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"servingPLMN"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct ASN_NGAP_NRNTNTAIInformation, tACListInNRNTN),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_TACListInNRNTN,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"tACListInNRNTN"
		},
	{ ATF_POINTER, 2, offsetof(struct ASN_NGAP_NRNTNTAIInformation, uELocationDerivedTACInNRNTN),
		(ASN_TAG_CLASS_CONTEXT | (2 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_TAC,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"uELocationDerivedTACInNRNTN"
		},
	{ ATF_POINTER, 1, offsetof(struct ASN_NGAP_NRNTNTAIInformation, iE_Extensions),
		(ASN_TAG_CLASS_CONTEXT | (3 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_ProtocolExtensionContainer_174P198,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"iE-Extensions"
		},
};
static const int asn_MAP_ASN_NGAP_NRNTNTAIInformation_oms_1[] = { 2, 3 };
static const ber_tlv_tag_t asn_DEF_ASN_NGAP_NRNTNTAIInformation_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static const asn_TYPE_tag2member_t asn_MAP_ASN_NGAP_NRNTNTAIInformation_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* servingPLMN */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 }, /* tACListInNRNTN */
    { (ASN_TAG_CLASS_CONTEXT | (2 << 2)), 2, 0, 0 }, /* uELocationDerivedTACInNRNTN */
    { (ASN_TAG_CLASS_CONTEXT | (3 << 2)), 3, 0, 0 } /* iE-Extensions */
};
asn_SEQUENCE_specifics_t asn_SPC_ASN_NGAP_NRNTNTAIInformation_specs_1 = {
	sizeof(struct ASN_NGAP_NRNTNTAIInformation),
	offsetof(struct ASN_NGAP_NRNTNTAIInformation, _asn_ctx),
	asn_MAP_ASN_NGAP_NRNTNTAIInformation_tag2el_1,
	4,	/* Count of tags in the map */
	asn_MAP_ASN_NGAP_NRNTNTAIInformation_oms_1,	/* Optional members */
	2, 0,	/* Root/Additions */
	4,	/* First extension addition */
};
asn_TYPE_descriptor_t asn_DEF_ASN_NGAP_NRNTNTAIInformation = {
	"NRNTNTAIInformation",
	"NRNTNTAIInformation",
	&asn_OP_SEQUENCE,
	asn_DEF_ASN_NGAP_NRNTNTAIInformation_tags_1,
	sizeof(asn_DEF_ASN_NGAP_NRNTNTAIInformation_tags_1)
		/sizeof(asn_DEF_ASN_NGAP_NRNTNTAIInformation_tags_1[0]), /* 1 */
	asn_DEF_ASN_NGAP_NRNTNTAIInformation_tags_1,	/* Same as above */
	sizeof(asn_DEF_ASN_NGAP_NRNTNTAIInformation_tags_1)
		/sizeof(asn_DEF_ASN_NGAP_NRNTNTAIInformation_tags_1[0]), /* 1 */
	{ 0, 0, SEQUENCE_constraint },
	asn_MBR_ASN_NGAP_NRNTNTAIInformation_1,
	4,	/* Elements count */
	&asn_SPC_ASN_NGAP_NRNTNTAIInformation_specs_1	/* Additional specs */
};

