/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "NGAP-IEs"
 * 	found in "NGAP-IEs.asn"
 * 	`asn1c -pdu=all -fcompound-names -findirect-choice -fno-include-deps -no-gen-OER -gen-PER -no-gen-example -D ngap`
 */

#include "ASN_NGAP_NPN-Support.h"

#include "ASN_NGAP_ProtocolIE-SingleContainer.h"
asn_per_constraints_t asn_PER_type_ASN_NGAP_NPN_Support_constr_1 CC_NOTUSED = {
	{ APC_CONSTRAINED,	 1,  1,  0,  1 }	/* (0..1) */,
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	0, 0	/* No PER value map */
};
asn_TYPE_member_t asn_MBR_ASN_NGAP_NPN_Support_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct ASN_NGAP_NPN_Support, choice.sNPN),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_NID,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"sNPN"
		},
	{ ATF_POINTER, 0, offsetof(struct ASN_NGAP_NPN_Support, choice.choice_Extensions),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_ProtocolIE_SingleContainer_126P42,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"choice-Extensions"
		},
};
static const asn_TYPE_tag2member_t asn_MAP_ASN_NGAP_NPN_Support_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* sNPN */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 } /* choice-Extensions */
};
asn_CHOICE_specifics_t asn_SPC_ASN_NGAP_NPN_Support_specs_1 = {
	sizeof(struct ASN_NGAP_NPN_Support),
	offsetof(struct ASN_NGAP_NPN_Support, _asn_ctx),
	offsetof(struct ASN_NGAP_NPN_Support, present),
	sizeof(((struct ASN_NGAP_NPN_Support *)0)->present),
	asn_MAP_ASN_NGAP_NPN_Support_tag2el_1,
	2,	/* Count of tags in the map */
	0, 0,
	-1	/* Extensions start */
};
asn_TYPE_descriptor_t asn_DEF_ASN_NGAP_NPN_Support = {
	"NPN-Support",
	"NPN-Support",
	&asn_OP_CHOICE,
	0,	/* No effective tags (pointer) */
	0,	/* No effective tags (count) */
	0,	/* No tags (pointer) */
	0,	/* No tags (count) */
	{ 0, &asn_PER_type_ASN_NGAP_NPN_Support_constr_1, CHOICE_constraint },
	asn_MBR_ASN_NGAP_NPN_Support_1,
	2,	/* Elements count */
	&asn_SPC_ASN_NGAP_NPN_Support_specs_1	/* Additional specs */
};

