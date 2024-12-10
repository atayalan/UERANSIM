/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "NGAP-IEs"
 * 	found in "NGAP-IEs.asn"
 * 	`asn1c -pdu=all -fcompound-names -findirect-choice -fno-include-deps -no-gen-OER -gen-PER -no-gen-example -D ngap`
 */

#include "ASN_NGAP_PathSwitchRequestAcknowledgeTransfer.h"

#include "ASN_NGAP_UPTransportLayerInformation.h"
#include "ASN_NGAP_SecurityIndication.h"
#include "ASN_NGAP_ProtocolExtensionContainer.h"
static asn_TYPE_member_t asn_MBR_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_1[] = {
	{ ATF_POINTER, 3, offsetof(struct ASN_NGAP_PathSwitchRequestAcknowledgeTransfer, uL_NGU_UP_TNLInformation),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		+1,	/* EXPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_UPTransportLayerInformation,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"uL-NGU-UP-TNLInformation"
		},
	{ ATF_POINTER, 2, offsetof(struct ASN_NGAP_PathSwitchRequestAcknowledgeTransfer, securityIndication),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_SecurityIndication,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"securityIndication"
		},
	{ ATF_POINTER, 1, offsetof(struct ASN_NGAP_PathSwitchRequestAcknowledgeTransfer, iE_Extensions),
		(ASN_TAG_CLASS_CONTEXT | (2 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_ProtocolExtensionContainer_174P207,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"iE-Extensions"
		},
};
static const int asn_MAP_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_oms_1[] = { 0, 1, 2 };
static const ber_tlv_tag_t asn_DEF_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static const asn_TYPE_tag2member_t asn_MAP_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* uL-NGU-UP-TNLInformation */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 }, /* securityIndication */
    { (ASN_TAG_CLASS_CONTEXT | (2 << 2)), 2, 0, 0 } /* iE-Extensions */
};
static asn_SEQUENCE_specifics_t asn_SPC_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_specs_1 = {
	sizeof(struct ASN_NGAP_PathSwitchRequestAcknowledgeTransfer),
	offsetof(struct ASN_NGAP_PathSwitchRequestAcknowledgeTransfer, _asn_ctx),
	asn_MAP_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_tag2el_1,
	3,	/* Count of tags in the map */
	asn_MAP_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_oms_1,	/* Optional members */
	3, 0,	/* Root/Additions */
	3,	/* First extension addition */
};
asn_TYPE_descriptor_t asn_DEF_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer = {
	"PathSwitchRequestAcknowledgeTransfer",
	"PathSwitchRequestAcknowledgeTransfer",
	&asn_OP_SEQUENCE,
	asn_DEF_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_tags_1,
	sizeof(asn_DEF_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_tags_1)
		/sizeof(asn_DEF_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_tags_1[0]), /* 1 */
	asn_DEF_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_tags_1,	/* Same as above */
	sizeof(asn_DEF_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_tags_1)
		/sizeof(asn_DEF_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_tags_1[0]), /* 1 */
	{ 0, 0, SEQUENCE_constraint },
	asn_MBR_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_1,
	3,	/* Elements count */
	&asn_SPC_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer_specs_1	/* Additional specs */
};

