#include "k5-int.h"

void KRB5_CALLCONV
krb5_get_init_creds_opt_init(krb5_get_init_creds_opt *opt)
{
   opt->flags = 0;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_tkt_life(krb5_get_init_creds_opt *opt, krb5_deltat tkt_life)
{
   opt->flags |= KRB5_GET_INIT_CREDS_OPT_TKT_LIFE;
   opt->tkt_life = tkt_life;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_renew_life(krb5_get_init_creds_opt *opt, krb5_deltat renew_life)
{
   opt->flags |= KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE;
   opt->renew_life = renew_life;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_forwardable(krb5_get_init_creds_opt *opt, int forwardable)
{
   opt->flags |= KRB5_GET_INIT_CREDS_OPT_FORWARDABLE;
   opt->forwardable = forwardable;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_proxiable(krb5_get_init_creds_opt *opt, int proxiable)
{
   opt->flags |= KRB5_GET_INIT_CREDS_OPT_PROXIABLE;
   opt->proxiable = proxiable;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_etype_list(krb5_get_init_creds_opt *opt, krb5_enctype *etype_list, int etype_list_length)
{
   opt->flags |= KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST;
   opt->etype_list = etype_list;
   opt->etype_list_length = etype_list_length;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_address_list(krb5_get_init_creds_opt *opt, krb5_address **addresses)
{
   opt->flags |= KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST;
   opt->address_list = addresses;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_preauth_list(krb5_get_init_creds_opt *opt, krb5_preauthtype *preauth_list, int preauth_list_length)
{
   opt->flags |= KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST;
   opt->preauth_list = preauth_list;
   opt->preauth_list_length = preauth_list_length;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_salt(krb5_get_init_creds_opt *opt, krb5_data *salt)
{
   opt->flags |= KRB5_GET_INIT_CREDS_OPT_SALT;
   opt->salt = salt;
}

/*
 * Extending the krb5_get_init_creds_opt structure.  The original
 * krb5_get_init_creds_opt structure is defined publicly.  The
 * new extended version is private.  The original interface
 * assumed a pre-allocated structure which was passed to
 * krb5_get_init_creds_init().  The new interface assumes that
 * the caller will call krb5_get_init_creds_alloc() and
 * krb5_get_init_creds_free().
 *
 * Callers MUST NOT call krb5_get_init_creds_init() after allocating an
 * opts structure using krb5_get_init_creds_alloc().  To do so will
 * introduce memory leaks.  Unfortunately, there is no way to enforce
 * this behavior.
 *
 * Two private flags are added for backward compatibility.
 * KRB5_GET_INIT_CREDS_OPT_EXTENDED says that the structure was allocated
 * with the new krb5_get_init_creds_opt_alloc() function.
 * KRB5_GET_INIT_CREDS_OPT_SHADOWED is set to indicate that the extended
 * structure is a shadow copy of an original krb5_get_init_creds_opt
 * structure.  
 * If KRB5_GET_INIT_CREDS_OPT_SHADOWED is set after a call to
 * krb5int_gic_opt_to_opte(), the resulting extended structure should be
 * freed (using krb5_get_init_creds_free).  Otherwise, the original
 * structure was already extended and there is no need to free it.
 */

/* Forward prototype */
static void
free_gic_opt_ext_preauth_data(krb5_context context,
			      krb5_gic_opt_ext *opte);

static krb5_error_code
krb5int_gic_opte_private_alloc(krb5_context context, krb5_gic_opt_ext *opte)
{
    if (NULL == opte || !krb5_gic_opt_is_extended(opte))
	return EINVAL;

    opte->opt_private = calloc(1, sizeof(*opte->opt_private));
    if (NULL == opte->opt_private) {
	return ENOMEM;
    }
    /* Allocate any private stuff */
    opte->opt_private->num_preauth_data = 0;
    opte->opt_private->preauth_data = NULL;
    return 0;
}

static krb5_error_code
krb5int_gic_opte_private_free(krb5_context context, krb5_gic_opt_ext *opte)
{
    if (NULL == opte || !krb5_gic_opt_is_extended(opte))
	return EINVAL;
	
    /* Free up any private stuff */
    if (opte->opt_private->preauth_data != NULL)
	free_gic_opt_ext_preauth_data(context, opte);
    free(opte->opt_private);
    opte->opt_private = NULL;
    return 0;
}

static krb5_gic_opt_ext *
krb5int_gic_opte_alloc(krb5_context context)
{
    krb5_gic_opt_ext *opte;
    krb5_error_code code;

    opte = calloc(1, sizeof(*opte));
    if (NULL == opte)
	return NULL;
    opte->flags = KRB5_GET_INIT_CREDS_OPT_EXTENDED;

    code = krb5int_gic_opte_private_alloc(context, opte);
    if (code) {
	krb5int_set_error(&context->err, code,
		"krb5int_gic_opte_alloc: krb5int_gic_opte_private_alloc failed");
	free(opte);
	return NULL;
    }
    return(opte);
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_alloc(krb5_context context,
			      krb5_get_init_creds_opt **opt)
{
    krb5_gic_opt_ext *opte;

    if (NULL == opt)
	return EINVAL;
    *opt = NULL;

    /*
     * We return a new extended structure cast as a krb5_get_init_creds_opt
     */
    opte = krb5int_gic_opte_alloc(context);
    if (NULL == opte)
	return ENOMEM;

    *opt = (krb5_get_init_creds_opt *) opte;
    return 0;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_free(krb5_context context,
			     krb5_get_init_creds_opt *opt)
{
    krb5_gic_opt_ext *opte;

    if (NULL == opt)
	return;

    /* Don't touch it if we didn't allocate it */
    if (!krb5_gic_opt_is_extended(opt))
	return;
    
    opte = (krb5_gic_opt_ext *)opt;
    if (opte->opt_private)
	krb5int_gic_opte_private_free(context, opte);

    free(opte);
}

static krb5_error_code
krb5int_gic_opte_copy(krb5_context context,
		      krb5_get_init_creds_opt *opt,
		      krb5_gic_opt_ext **opte)
{
    krb5_gic_opt_ext *oe;

    oe = krb5int_gic_opte_alloc(context);
    if (NULL == oe)
	return ENOMEM;
    memcpy(oe, opt, sizeof(*opt));
    /* Fix these -- overwritten by the copy */
    oe->flags |= ( KRB5_GET_INIT_CREDS_OPT_EXTENDED |
		   KRB5_GET_INIT_CREDS_OPT_SHADOWED);

    *opte = oe;
    return 0;
}

/*
 * Convert a krb5_get_init_creds_opt pointer to a pointer to
 * an extended, krb5_gic_opt_ext pointer.  If the original
 * pointer already points to an extended structure, then simply
 * return the original pointer.  Otherwise, if 'force' is non-zero,
 * allocate an extended structure and copy the original over it.
 * If the original pointer did not point to an extended structure
 * and 'force' is zero, then return an error.  This is used in
 * cases where the original *should* be an extended structure.
 */
krb5_error_code
krb5int_gic_opt_to_opte(krb5_context context,
			krb5_get_init_creds_opt *opt,
			krb5_gic_opt_ext **opte,
			unsigned int force,
			const char *where)
{
    if (!krb5_gic_opt_is_extended(opt)) {
	if (force) {
	    return krb5int_gic_opte_copy(context, opt, opte);
	} else {
	    krb5int_set_error(&context->err, EINVAL,
		    "%s: attempt to convert non-extended krb5_get_init_creds_opt",
		    where);
	    return EINVAL;
	}
    }
    /* If it is already extended, just return it */
    *opte = (krb5_gic_opt_ext *)opt;
    return 0;
}

static void
free_gic_opt_ext_preauth_data(krb5_context context,
			      krb5_gic_opt_ext *opte)
{
    int i;

    if (NULL == opte || !krb5_gic_opt_is_extended(opte))
	return;
    if (NULL == opte->opt_private || NULL == opte->opt_private->preauth_data)
	return;

    for (i = 0; i < opte->opt_private->num_preauth_data; i++) {
	if (opte->opt_private->preauth_data[i].attr != NULL)
	    free(opte->opt_private->preauth_data[i].attr);
	if (opte->opt_private->preauth_data[i].value != NULL)
	    free(opte->opt_private->preauth_data[i].value);
    }
    free(opte->opt_private->preauth_data);
    opte->opt_private->preauth_data = NULL;
    opte->opt_private->num_preauth_data = 0;
}

static krb5_error_code
add_gic_opt_ext_preauth_data(krb5_context context,
			     krb5_gic_opt_ext *opte,
			     int num_preauth_data,
			     krb5_gic_opt_pa_data *preauth_data)
{
    size_t newsize;
    int i, j;
    krb5_gic_opt_pa_data *newpad;

    newsize = opte->opt_private->num_preauth_data + num_preauth_data;
    newsize = newsize * sizeof(*preauth_data);
    if (opte->opt_private->preauth_data == NULL)
	newpad = malloc(newsize);
    else
	newpad = realloc(opte->opt_private->preauth_data, newsize);
    if (newpad == NULL)
	return ENOMEM;

    j = opte->opt_private->num_preauth_data;
    for (i = 0; i < num_preauth_data; i++) {
	newpad[j+i].pa_type = -1;
	newpad[j+i].attr = NULL;
	newpad[j+i].value = NULL;
    }
    for (i = 0; i < num_preauth_data; i++) {
	newpad[j+i].pa_type = preauth_data[i].pa_type;
	newpad[j+i].attr = strdup(preauth_data[i].attr);
	newpad[j+i].value = strdup(preauth_data[i].value);
	if (newpad[j+i].attr == NULL || newpad[j+i].value == NULL)
	    goto cleanup;
    }
    opte->opt_private->num_preauth_data = j+i;
    opte->opt_private->preauth_data = newpad;
    return 0;

cleanup:
    for (i = num_preauth_data; i >= 0; i--) {
	if (newpad[j+i].value != NULL)
	    free(newpad[j+i].value);
	if (newpad[j+i].attr != NULL)
	    free(newpad[j+i].attr);
    }
    return ENOMEM;
}

/*
 * This function allows the caller to supply options to preauth
 * plugins.  Preauth plugin modules are given a chance to look
 * at the options at the time this function is called to check
 * the validity of its options.
 * The 'opt' pointer supplied to this function must have been
 * obtained using krb5_get_init_creds_opt_alloc()
 */
krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_pa(krb5_context context,
			       krb5_get_init_creds_opt *opt,
			       krb5_principal principal,
			       const char *password,
			       krb5_prompter_fct prompter,
			       void *prompter_data,
			       int num_preauth_data,
			       krb5_gic_opt_pa_data *preauth_data)
{
    krb5_error_code retval;
    krb5_gic_opt_ext *opte;

    retval = krb5int_gic_opt_to_opte(context, opt, &opte, 0,
				     "krb5_get_init_creds_opt_set_pkinit");
    if (retval)
	return retval;

    if (num_preauth_data <= 0) {
	retval = EINVAL;
	krb5int_set_error(&context->err, retval,
		"krb5_get_init_creds_opt_set_pa: "
		"num_preauth_data of %d is invalid", num_preauth_data);
	return retval;
    }

    /*
     * Copy all the options into the extended get_init_creds_opt structure
     */
    retval = add_gic_opt_ext_preauth_data(context, opte,
					  num_preauth_data, preauth_data);
    if (retval)
	return retval;

    /*
     * Give the plugins a chance at the options now.  Note that only
     * the new options are passed to the plugins.  They should have
     * already had a chance at any pre-existing options.
     */
    retval = krb5_preauth_supply_preauth_data(context, opte, principal,
					      password, prompter,
					      prompter_data, num_preauth_data,
					      preauth_data);
    return retval;
}

static int
pa_data_applies(krb5_context context, int num_pa_types,
		krb5_preauthtype *pa_types, krb5_gic_opt_pa_data *preauth_data)
{
    int i;
    for (i = 0; i < num_pa_types; i++) {
	if (preauth_data->pa_type == pa_types[i])
	    return 1;
    }
    return 0;
}

/*
 * This function allows a preauth plugin to obtain preauth
 * options. Only options which are applicable to the pa_types
 * which the plugin module claims to support (pa_types) are
 * returned.  The preauth_data returned from this function
 * should be freed by calling krb5_get_init_creds_opt_free_pa().
 * The 'opt' pointer supplied to this function must have been
 * obtained using krb5_get_init_creds_opt_alloc()
 */
krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_get_pa(krb5_context context,
			       krb5_get_init_creds_opt *opt,
			       int num_pa_types,
			       krb5_preauthtype *pa_types,
			       int *num_preauth_data,
			       krb5_gic_opt_pa_data **preauth_data)
{
    krb5_error_code retval = ENOMEM;
    krb5_gic_opt_ext *opte;
    krb5_gic_opt_pa_data *p = NULL;
    int i, j;
    size_t allocsize;

    retval = krb5int_gic_opt_to_opte(context, opt, &opte, 0,
				     "krb5_get_init_creds_opt_get_pkinit");
    if (retval)
	return retval;

    *num_preauth_data = 0;
    *preauth_data = NULL;

    /* The most we could return is all of them */
    allocsize =
	    opte->opt_private->num_preauth_data * sizeof(krb5_gic_opt_pa_data);
    p = malloc(allocsize);
    if (p == NULL)
	return retval;

    /* Init these to make cleanup easier */
    for (i = 0; i < opte->opt_private->num_preauth_data; i++) {
	p[i].attr = NULL;
	p[i].value = NULL;
    }

    j = 0;
    for (i = 0; i < opte->opt_private->num_preauth_data; i++) {
	if (pa_data_applies(context, num_pa_types, pa_types,
			    &opte->opt_private->preauth_data[i])) {
	    p[j].pa_type = opte->opt_private->preauth_data[i].pa_type;
	    p[j].attr = strdup(opte->opt_private->preauth_data[i].attr);
	    p[j].value = strdup(opte->opt_private->preauth_data[i].value);
	    if (p[j].attr == NULL || p[j].value == NULL)
		goto cleanup;
	    j++;
	}
    }
    if (j == 0) {
	retval = ENOENT;
	goto cleanup;
    }
    *num_preauth_data = j;
    *preauth_data = p;
    return 0;
cleanup:
    for (i = 0; i < opte->opt_private->num_preauth_data; i++) {
	if (p[i].attr != NULL)
	    free(p[i].attr);
	if (p[i].value != NULL)
	    free(p[i].value);
    }
    free(p);
    return retval;
}

/*
 * This function frees the preauth_data that was returned by 
 * krb5_get_init_creds_opt_get_pa().
 */
void KRB5_CALLCONV
krb5_get_init_creds_opt_free_pa(krb5_context context,
				int num_preauth_data,
				krb5_gic_opt_pa_data *preauth_data)
{
    int i;

    if (num_preauth_data <= 0 || preauth_data == NULL)
	return;

    for (i = 0; i < num_preauth_data; i++) {
	if (preauth_data[i].attr != NULL)
	    free(preauth_data[i].attr);
	if (preauth_data[i].value != NULL)
	    free(preauth_data[i].value);
    }
    free(preauth_data);
}


krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_pkinit(krb5_context context,
				   krb5_get_init_creds_opt *opt,
				   krb5_principal principal,
				   const char *x509_user_identity,
				   const char *x509_anchors,
				   char * const * x509_chain_list,
				   char * const * x509_revoke_list,
				   int flags,
				   krb5_prompter_fct prompter,
				   void *prompter_data,
				   char *password)
{
    krb5_gic_opt_pa_data *pad;
    int num_pad = 0;
    int i, j;
    krb5_error_code retval;

    /* Figure out how many preauth data structs we'll need */
    if (x509_user_identity != NULL)
	num_pad++;
    if (x509_anchors != NULL)
	num_pad++;
    if (x509_chain_list != NULL)
	for (j = 0; x509_chain_list[j] != NULL; j++)
	    num_pad++;
    if (x509_revoke_list != NULL)
	for (j = 0; x509_revoke_list[j] != NULL; j++)
	    num_pad++;
    if (flags != 0) {
	/* XXX should be more generic? What other flags are there? */
#define PKINIT_RSA_PROTOCOL 0x00000002
	if (flags & PKINIT_RSA_PROTOCOL)
	    num_pad++;
    }
	

    /* Allocate the krb5_gic_opt_pa_data structures and populate */
    pad = malloc(num_pad * sizeof(krb5_gic_opt_pa_data));
    if (pad == NULL)
	return ENOMEM;

    i = 0;
    if (x509_user_identity != NULL) {
	pad[i].pa_type = KRB5_PADATA_PK_AS_REQ;
	pad[i].attr = "X509_user_identity";
	pad[i].value = x509_user_identity;
	i++;
    }
    if (x509_anchors != NULL) {
	pad[i].pa_type = KRB5_PADATA_PK_AS_REQ;
	pad[i].attr = "X509_anchors";
	pad[i].value = x509_anchors;
	i++;
    }
    if (x509_chain_list != NULL) {
	for (j = 0; x509_chain_list[j] != NULL; j++) {
	    pad[i].pa_type = KRB5_PADATA_PK_AS_REQ;
	    pad[i].attr = "X509_chain_list";
	    pad[i].value = x509_chain_list[j];
	    i++;
	}
    }
    if (x509_revoke_list != NULL) {
	for (j = 0; x509_revoke_list[j] != NULL; j++) {
	    pad[i].pa_type = KRB5_PADATA_PK_AS_REQ;
	    pad[i].attr = "X509_revoke_list";
	    pad[i].value = x509_revoke_list[j];
	    i++;
	}
    }
    if (flags != 0) {
	if (flags & PKINIT_RSA_PROTOCOL) {
	    pad[i].pa_type = KRB5_PADATA_PK_AS_REQ;
	    pad[i].attr = "flag_RSA_PROTOCOL";
	    pad[i].value = "yes";
	    i++;
	}
    }

    retval = krb5_get_init_creds_opt_set_pa(context, opt, principal, password,
					    prompter, prompter_data, i, pad);

    free(pad);
    return retval;
}
