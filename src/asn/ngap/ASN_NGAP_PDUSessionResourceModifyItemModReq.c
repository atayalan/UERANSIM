/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "NGAP-IEs"
 * 	found in "NGAP-IEs.asn"
 * 	`asn1c -pdu=all -fcompound-names -findirect-choice -fno-include-deps -no-gen-OER -gen-PER -no-gen-example -D ngap`
 */

#include "ASN_NGAP_PDUSessionResourceModifyItemModReq.h"

#include "ASN_NGAP_ProtocolExtensionContainer.h"
static int
memb_ASN_NGAP_pDUSessionResourceModifyRequestTransfer_constraint_1(const asn_TYPE_descriptor_t *td, const void *sptr,
			asn_app_constraint_failed_f *ctfailcb, void *app_key) {
	const OCTET_STRING_t *st = (const OCTET_STRING_t *)sptr;
	
	if(!sptr) {
		ASN__CTFAIL(app_key, td, sptr,
			"%s: value not given (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}
	
	
	if(1 /* No applicable constraints whatsoever */) {
		(void)st; /* Unused variable */
		/* Nothing is here. See below */
	}
	
	return td->encoding_constraints.general_constraints(td, sptr, ctfailcb, app_key);
}

static asn_per_constraints_t asn_PER_memb_ASN_NGAP_pDUSessionResourceModifyRequestTransfer_constr_4 CC_NOTUSED = {
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	{ APC_SEMI_CONSTRAINED,	-1, -1,  0,  0 }	/* (SIZE(0..MAX)) */,
	0, 0	/* No PER value map */
};
asn_TYPE_member_t asn_MBR_ASN_NGAP_PDUSessionResourceModifyItemModReq_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct ASN_NGAP_PDUSessionResourceModifyItemModReq, pDUSessionID),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_PDUSessionID,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"pDUSessionID"
		},
	{ ATF_POINTER, 1, offsetof(struct ASN_NGAP_PDUSessionResourceModifyItemModReq, nAS_PDU),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_NAS_PDU,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"nAS-PDU"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct ASN_NGAP_PDUSessionResourceModifyItemModReq, pDUSessionResourceModifyRequestTransfer),
		(ASN_TAG_CLASS_CONTEXT | (2 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_OCTET_STRING,
		0,
		{ 0, &asn_PER_memb_ASN_NGAP_pDUSessionResourceModifyRequestTransfer_constr_4,  memb_ASN_NGAP_pDUSessionResourceModifyRequestTransfer_constraint_1 },
		0, 0, /* No default value */
		"pDUSessionResourceModifyRequestTransfer"
		},
	{ ATF_POINTER, 1, offsetof(struct ASN_NGAP_PDUSessionResourceModifyItemModReq, iE_Extensions),
		(ASN_TAG_CLASS_CONTEXT | (3 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_ASN_NGAP_ProtocolExtensionContainer_174P236,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"iE-Extensions"
		},
};
static const int asn_MAP_ASN_NGAP_PDUSessionResourceModifyItemModReq_oms_1[] = { 1, 3 };
static const ber_tlv_tag_t asn_DEF_ASN_NGAP_PDUSessionResourceModifyItemModReq_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static const asn_TYPE_tag2member_t asn_MAP_ASN_NGAP_PDUSessionResourceModifyItemModReq_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* pDUSessionID */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 }, /* nAS-PDU */
    { (ASN_TAG_CLASS_CONTEXT | (2 << 2)), 2, 0, 0 }, /* pDUSessionResourceModifyRequestTransfer */
    { (ASN_TAG_CLASS_CONTEXT | (3 << 2)), 3, 0, 0 } /* iE-Extensions */
};
asn_SEQUENCE_specifics_t asn_SPC_ASN_NGAP_PDUSessionResourceModifyItemModReq_specs_1 = {
	sizeof(struct ASN_NGAP_PDUSessionResourceModifyItemModReq),
	offsetof(struct ASN_NGAP_PDUSessionResourceModifyItemModReq, _asn_ctx),
	asn_MAP_ASN_NGAP_PDUSessionResourceModifyItemModReq_tag2el_1,
	4,	/* Count of tags in the map */
	asn_MAP_ASN_NGAP_PDUSessionResourceModifyItemModReq_oms_1,	/* Optional members */
	2, 0,	/* Root/Additions */
	4,	/* First extension addition */
};
asn_TYPE_descriptor_t asn_DEF_ASN_NGAP_PDUSessionResourceModifyItemModReq = {
	"PDUSessionResourceModifyItemModReq",
	"PDUSessionResourceModifyItemModReq",
	&asn_OP_SEQUENCE,
	asn_DEF_ASN_NGAP_PDUSessionResourceModifyItemModReq_tags_1,
	sizeof(asn_DEF_ASN_NGAP_PDUSessionResourceModifyItemModReq_tags_1)
		/sizeof(asn_DEF_ASN_NGAP_PDUSessionResourceModifyItemModReq_tags_1[0]), /* 1 */
	asn_DEF_ASN_NGAP_PDUSessionResourceModifyItemModReq_tags_1,	/* Same as above */
	sizeof(asn_DEF_ASN_NGAP_PDUSessionResourceModifyItemModReq_tags_1)
		/sizeof(asn_DEF_ASN_NGAP_PDUSessionResourceModifyItemModReq_tags_1[0]), /* 1 */
	{ 0, 0, SEQUENCE_constraint },
	asn_MBR_ASN_NGAP_PDUSessionResourceModifyItemModReq_1,
	4,	/* Elements count */
	&asn_SPC_ASN_NGAP_PDUSessionResourceModifyItemModReq_specs_1	/* Additional specs */
};

