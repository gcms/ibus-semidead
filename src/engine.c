/* vim:set et sts=4: */

#include "engine.h"

typedef struct _IBusSemiDeadEngine IBusSemiDeadEngine;
typedef struct _IBusSemiDeadEngineClass IBusSemiDeadEngineClass;

struct _IBusSemiDeadEngine {
    IBusEngine parent;

    /* members */
    GString *preedit;

    GNode *root;
    GNode *cur_node;
};

struct _IBusSemiDeadEngineClass {
    IBusEngineClass parent;
};

/* functions prototype */
static void	ibus_semidead_engine_class_init	(IBusSemiDeadEngineClass	*klass);
static void	ibus_semidead_engine_init		(IBusSemiDeadEngine		*engine);
static void	ibus_semidead_engine_destroy		(IBusSemiDeadEngine		*engine);
static gboolean
ibus_semidead_engine_process_key_event
        (IBusEngine             *engine,
         guint               	 keyval,
         guint               	 keycode,
         guint               	 modifiers);


static void ibus_semidead_engine_commit_string
        (IBusSemiDeadEngine      *sdengine,
         const gchar            *string);

static gboolean
ibus_semidead_engine_match_keyval
        (IBusSemiDeadEngine *sdengine,
         guint keyval);

G_DEFINE_TYPE (IBusSemiDeadEngine, ibus_semidead_engine, IBUS_TYPE_ENGINE)

static void
ibus_semidead_engine_class_init (IBusSemiDeadEngineClass *klass)
{
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS (klass);
    IBusEngineClass *engine_class = IBUS_ENGINE_CLASS (klass);

    ibus_object_class->destroy = (IBusObjectDestroyFunc) ibus_semidead_engine_destroy;

    engine_class->process_key_event = ibus_semidead_engine_process_key_event;
}

static void ibus_semidead_engine_init_tree(GNode *root,
                                          const gchar *keyval_name,
                                          const gchar *from,
                                          const gchar *to) {
    glong len = strlen(from);
    g_return_if_fail(len == g_utf8_strlen(to, strlen(to)));

    guint keyval = ibus_keyval_from_name(keyval_name);
    g_return_if_fail(keyval);

    GNode *symbol_node = g_node_append_data(root, keyval);

    gchar vowel_keyval_name[2];
    while (*from) {
        *vowel_keyval_name = *from;

        GNode *vowel_node = g_node_append_data(symbol_node, ibus_keyval_from_name(vowel_keyval_name));
        g_node_append_data(vowel_node, g_utf8_get_char(to));

        from++;
        to = g_utf8_next_char(to);
    }
}

static void
ibus_semidead_engine_init (IBusSemiDeadEngine *sdengine)
{
    sdengine->preedit = g_string_new ("");

    sdengine->root = g_node_new (NULL);

    ibus_semidead_engine_init_tree(sdengine->root,
                                  "apostrophe",
                                  "aeioucAEIOUC",
                                  "áéíóúçÁÉÍÓÚÇ");

    ibus_semidead_engine_init_tree(sdengine->root,
                                  "quotedbl",
                                  "aeiouAEIOU",
                                  "äëïöüÄËÏÖÜ");


    ibus_semidead_engine_init_tree(sdengine->root,
                                  "asciitilde",
                                  "aonAON",
                                  "ãõñÃÕÑ");

    ibus_semidead_engine_init_tree(sdengine->root,
                                  "asciicircum",
                                  "aeiouAEIOU",
                                  "âêîôûÂÊÎÔÛ");

    ibus_semidead_engine_init_tree(sdengine->root,
                                  "grave",
                                  "aeiouAEIOU",
                                  "àèìòùÀÈÌÒÙ");


    sdengine->cur_node = sdengine->root;
}

static void
ibus_semidead_engine_destroy (IBusSemiDeadEngine *sdengine)
{
    if (sdengine->preedit) {
        g_string_free (sdengine->preedit, TRUE);
        sdengine->preedit = NULL;
    }

    if (sdengine->root)
        g_node_destroy(sdengine->root);

    ((IBusObjectClass *) ibus_semidead_engine_parent_class)->destroy ((IBusObject *) sdengine);
}


static void
ibus_semidead_engine_update_preedit_text (IBusSemiDeadEngine *sdengine,
                                          GString *preedit)
{
    IBusText *text = ibus_text_new_from_static_string (preedit->str);

    text->attrs = ibus_attr_list_new ();
    ibus_attr_list_append (text->attrs,
                           ibus_attr_underline_new (IBUS_ATTR_UNDERLINE_SINGLE, 0, sdengine->preedit->len));

    ibus_engine_update_preedit_text ((IBusEngine *)sdengine, text, preedit->len, TRUE);

}

static void
ibus_semidead_engine_update_preedit (IBusSemiDeadEngine *sdengine)
{
    ibus_semidead_engine_update_preedit_text (sdengine, sdengine->preedit);
}

static void
ibus_semidead_engine_clean_preedit (IBusSemiDeadEngine *sdengine) {
    g_string_assign(sdengine->preedit, "");
    ibus_engine_hide_preedit_text((IBusEngine *) sdengine);
}



static void
ibus_semidead_engine_commit_string (IBusSemiDeadEngine *sdengine,
                                    const gchar       *string)
{
    IBusText *text = ibus_text_new_from_static_string (string);
    ibus_engine_commit_text ((IBusEngine *) sdengine, text);
}


/* commit preedit to client and update preedit */
static gboolean
ibus_semidead_engine_commit_preedit (IBusSemiDeadEngine *sdengine)
{
    if (sdengine->preedit->len == 0)
        return FALSE;

    ibus_semidead_engine_commit_string (sdengine, sdengine->preedit->str);
    ibus_semidead_engine_clean_preedit(sdengine);

    return TRUE;
}

static gboolean
ibus_semidead_engine_match_keyval(IBusSemiDeadEngine *sdengine,
                                           guint keyval) {
    GNode *node = g_node_find_child(sdengine->cur_node, G_TRAVERSE_NON_LEAVES, keyval);

    if (node == NULL)
        return FALSE;

    if (g_node_n_children(node) == 1 && G_NODE_IS_LEAF(g_node_first_child(node))) {
        g_string_assign(sdengine->preedit, ""); // use preedit as a tmp buffer
        g_string_append_unichar(sdengine->preedit, g_node_first_child(node)->data);
        ibus_semidead_engine_commit_string(sdengine, sdengine->preedit->str);

        ibus_semidead_engine_clean_preedit(sdengine);

        sdengine->cur_node = sdengine->root;
    } else {
        g_string_append_c(sdengine->preedit, keyval);
        ibus_semidead_engine_update_preedit(sdengine);

        sdengine->cur_node = node;
    }

    return TRUE;
}

static gboolean
ibus_semidead_engine_process_key_event (IBusEngine *engine,
                                       guint       keyval,
                                       guint       keycode,
                                       guint       modifiers)
{
    IBusSemiDeadEngine *sdengine = (IBusSemiDeadEngine *)engine;

    if (modifiers & IBUS_RELEASE_MASK)
        return FALSE;

    if (!ibus_keyval_to_unicode(keyval))
	    return FALSE;

    if (ibus_semidead_engine_match_keyval(sdengine, keyval))
        return TRUE;

    gboolean in_sequence = sdengine->cur_node != sdengine->root;
    gboolean exit_sequence = in_sequence && keyval == IBUS_KEY_space;

    sdengine->cur_node = sdengine->root;
    ibus_semidead_engine_commit_preedit(sdengine);

    return exit_sequence || ibus_semidead_engine_match_keyval(sdengine, keyval);
}
