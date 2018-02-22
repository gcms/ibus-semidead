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
ibus_semidead_engine_process_key_event_node
        (IBusSemiDeadEngine *sdengine,
         guint keyval);
/*
static void ibus_semidead_engine_update      (IBusSemiDeadEngine      *sdengine);
*/


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
                                          const gchar *keyval,
                                          const gchar *from,
                                          const gchar *to) {

    GNode *lead = g_node_append_data(root, ibus_keyval_from_name(keyval));

    gchar keyval_name[2];
    keyval_name[1] = NULL;

    for (int i = 0; i < strlen(from); i++) {
        keyval_name[0] = from[i];

        GNode *letra = g_node_append_data(lead, ibus_keyval_from_name(keyval_name));
        g_node_append_data(letra, g_utf8_substring(to, i, i+1));
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

    ((IBusObjectClass *) ibus_semidead_engine_parent_class)->destroy ((IBusObject *)sdengine);
}

static void
ibus_semidead_engine_update_preedit (IBusSemiDeadEngine *sdengine)
{
    IBusText *text = ibus_text_new_from_static_string (sdengine->preedit->str);

    text->attrs = ibus_attr_list_new ();
    ibus_attr_list_append (text->attrs,
                           ibus_attr_underline_new (IBUS_ATTR_UNDERLINE_SINGLE, 0, sdengine->preedit->len));

    ibus_engine_update_preedit_text ((IBusEngine *)sdengine, text, sdengine->preedit->len, TRUE);

}

/* commit preedit to client and update preedit */
static gboolean
ibus_semidead_engine_commit_preedit (IBusSemiDeadEngine *sdengine)
{
    if (sdengine->preedit->len == 0)
        return FALSE;

    ibus_semidead_engine_commit_string (sdengine, sdengine->preedit->str);
    g_string_assign (sdengine->preedit, "");

    ibus_semidead_engine_update_preedit(sdengine);

    return TRUE;
}


static void
ibus_semidead_engine_commit_string (IBusSemiDeadEngine *sdengine,
                                   const gchar       *string)
{
    IBusText *text;
    text = ibus_text_new_from_static_string (string);
    ibus_engine_commit_text ((IBusEngine *)sdengine, text);
}

static gboolean
ibus_semidead_engine_process_key_event_node(IBusSemiDeadEngine *sdengine,
                                           guint keyval) {
    GNode *node = g_node_find_child(sdengine->cur_node, G_TRAVERSE_ALL, keyval);

    if (node == NULL)
        return FALSE;

    if (g_node_n_children(node) == 1 && G_NODE_IS_LEAF(g_node_first_child(node))) {
//        g_string_printf(sdengine->preedit, "%c", g_node_first_child(node)->data);
//        ibus_semidead_engine_commit_string(sdengine, sdengine->preedit->str);
        ibus_semidead_engine_commit_string(sdengine, g_node_first_child(node)->data);

        g_string_assign(sdengine->preedit, "");
        ibus_semidead_engine_update_preedit(sdengine);

        sdengine->cur_node = sdengine->root;
        ibus_semidead_engine_commit_preedit(sdengine);
    } else {
        sdengine->cur_node = node;

        g_string_append_c(sdengine->preedit, keyval);
        ibus_semidead_engine_update_preedit(sdengine);
    }

    return TRUE;
}

#define is_alpha(c) (((c) >= IBUS_a && (c) <= IBUS_z) || ((c) >= IBUS_A && (c) <= IBUS_Z))
#define is_printable(c) (is_alpha((c)) || (c) == IBUS_space)

static gboolean
ibus_semidead_engine_process_key_event (IBusEngine *engine,
                                       guint       keyval,
                                       guint       keycode,
                                       guint       modifiers)
{
    IBusText *text;
    IBusSemiDeadEngine *sdengine = (IBusSemiDeadEngine *)engine;


    if (modifiers & IBUS_RELEASE_MASK)
        return FALSE;

//    GFile *file = g_file_new_for_path ("/tmp/keyval");
//    GOutputStream *os = g_file_append_to(file, G_FILE_CREATE_NONE, NULL, NULL);
//    g_output_stream_printf (os, NULL, NULL, NULL, "%d %d %d %c %s\r\n", keyval, keycode, modifiers, ibus_keyval_to_unicode(keyval), ibus_keyval_name(keyval));
//    g_object_unref(os);
//    g_object_unref(file);

    if (!ibus_keyval_to_unicode(keyval))
	    return FALSE;

    if (ibus_semidead_engine_process_key_event_node(sdengine, keyval))
        return TRUE;

    gboolean handled = sdengine->cur_node != sdengine->root && keyval == IBUS_KEY_space;

    sdengine->cur_node = sdengine->root;
    ibus_semidead_engine_commit_preedit(sdengine);

    if (handled)
        return TRUE;

    if (ibus_semidead_engine_process_key_event_node(sdengine, keyval))
        return TRUE;

    return FALSE;
}
