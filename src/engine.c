/* vim:set et sts=4: */

//#include <enchant.h>
#include "engine.h"

typedef struct _IBusEnchantEngine IBusEnchantEngine;
typedef struct _IBusEnchantEngineClass IBusEnchantEngineClass;

struct _IBusEnchantEngine {
    IBusEngine parent;

    /* members */
    GString *preedit;
    GNode *root;

    gint cursor_pos;
    GNode *cur_node;
};

struct _IBusEnchantEngineClass {
    IBusEngineClass parent;
};

/* functions prototype */
static void	ibus_enchant_engine_class_init	(IBusEnchantEngineClass	*klass);
static void	ibus_enchant_engine_init		(IBusEnchantEngine		*engine);
static void	ibus_enchant_engine_destroy		(IBusEnchantEngine		*engine);
static gboolean
ibus_enchant_engine_process_key_event
        (IBusEngine             *engine,
         guint               	 keyval,
         guint               	 keycode,
         guint               	 modifiers);


static void ibus_enchant_engine_commit_string
        (IBusEnchantEngine      *enchant,
         const gchar            *string);

static gboolean
ibus_enchant_engine_process_key_event_node
        (IBusEnchantEngine *enchant,
         guint keyval);
/*
static void ibus_enchant_engine_update      (IBusEnchantEngine      *enchant);
*/


G_DEFINE_TYPE (IBusEnchantEngine, ibus_enchant_engine, IBUS_TYPE_ENGINE)

static void
ibus_enchant_engine_class_init (IBusEnchantEngineClass *klass)
{
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS (klass);
    IBusEngineClass *engine_class = IBUS_ENGINE_CLASS (klass);

    ibus_object_class->destroy = (IBusObjectDestroyFunc) ibus_enchant_engine_destroy;

    engine_class->process_key_event = ibus_enchant_engine_process_key_event;
}

static void ibus_enchant_engine_init_tree(GNode *root,
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
ibus_enchant_engine_init (IBusEnchantEngine *enchant)
{
    enchant->preedit = g_string_new ("");
    enchant->cursor_pos = 0;

    enchant->root = g_node_new (NULL);

    ibus_enchant_engine_init_tree(enchant->root,
                                  "apostrophe",
                                  "aeioucAEIOUC",
                                  "áéíóúçÁÉÍÓÚÇ");

    ibus_enchant_engine_init_tree(enchant->root,
                                  "quotedbl",
                                  "aeiouAEIOU",
                                  "äëïöüÄËÏÖÜ");


    ibus_enchant_engine_init_tree(enchant->root,
                                  "asciitilde",
                                  "aonAON",
                                  "ãõñÃÕÑ");

    ibus_enchant_engine_init_tree(enchant->root,
                                  "asciicircum",
                                  "aeiouAEIOU",
                                  "âêîôûÂÊÎÔÛ");

    ibus_enchant_engine_init_tree(enchant->root,
                                  "grave",
                                  "aeiouAEIOU",
                                  "àèìòùÀÈÌÒÙ");


    enchant->cur_node = enchant->root;
}

static void
ibus_enchant_engine_destroy (IBusEnchantEngine *enchant)
{
    if (enchant->preedit) {
        g_string_free (enchant->preedit, TRUE);
        enchant->preedit = NULL;
    }

    ((IBusObjectClass *) ibus_enchant_engine_parent_class)->destroy ((IBusObject *)enchant);
}

static void
ibus_enchant_engine_update_preedit (IBusEnchantEngine *enchant)
{
    IBusText *text = ibus_text_new_from_static_string (enchant->preedit->str);

    text->attrs = ibus_attr_list_new ();
    ibus_attr_list_append (text->attrs,
                           ibus_attr_underline_new (IBUS_ATTR_UNDERLINE_SINGLE, 0, enchant->preedit->len));

    ibus_engine_update_preedit_text ((IBusEngine *)enchant, text, enchant->preedit->len, TRUE);

}

/* commit preedit to client and update preedit */
static gboolean
ibus_enchant_engine_commit_preedit (IBusEnchantEngine *enchant)
{
    if (enchant->preedit->len == 0)
        return FALSE;

    ibus_enchant_engine_commit_string (enchant, enchant->preedit->str);
    g_string_assign (enchant->preedit, "");
    enchant->cursor_pos = 0;

    ibus_enchant_engine_update_preedit(enchant);

    return TRUE;
}


static void
ibus_enchant_engine_commit_string (IBusEnchantEngine *enchant,
                                   const gchar       *string)
{
    IBusText *text;
    text = ibus_text_new_from_static_string (string);
    ibus_engine_commit_text ((IBusEngine *)enchant, text);
}

static gboolean
ibus_enchant_engine_process_key_event_node(IBusEnchantEngine *enchant,
                                           guint keyval) {
    GNode *node = g_node_find_child(enchant->cur_node, G_TRAVERSE_ALL, keyval);

    if (node == NULL)
        return FALSE;

    if (g_node_n_children(node) == 1 && G_NODE_IS_LEAF(g_node_first_child(node))) {
//        g_string_printf(enchant->preedit, "%c", g_node_first_child(node)->data);
//        ibus_enchant_engine_commit_string(enchant, enchant->preedit->str);
        ibus_enchant_engine_commit_string(enchant, g_node_first_child(node)->data);

        g_string_assign(enchant->preedit, "");
        enchant->cursor_pos = 0;
        ibus_enchant_engine_update_preedit(enchant);

        enchant->cur_node = enchant->root;
        ibus_enchant_engine_commit_preedit(enchant);
    } else {
        enchant->cur_node = node;

        g_string_append_c(enchant->preedit, keyval);
        enchant->cursor_pos++;
        ibus_enchant_engine_update_preedit(enchant);
    }

    return TRUE;
}

#define is_alpha(c) (((c) >= IBUS_a && (c) <= IBUS_z) || ((c) >= IBUS_A && (c) <= IBUS_Z))
#define is_printable(c) (is_alpha((c)) || (c) == IBUS_space)

static gboolean
ibus_enchant_engine_process_key_event (IBusEngine *engine,
                                       guint       keyval,
                                       guint       keycode,
                                       guint       modifiers)
{
    IBusText *text;
    IBusEnchantEngine *enchant = (IBusEnchantEngine *)engine;


    if (modifiers & IBUS_RELEASE_MASK)
        return FALSE;

//    GFile *file = g_file_new_for_path ("/tmp/keyval");
//    GOutputStream *os = g_file_append_to(file, G_FILE_CREATE_NONE, NULL, NULL);
//    g_output_stream_printf (os, NULL, NULL, NULL, "%d %d %d %c %s\r\n", keyval, keycode, modifiers, ibus_keyval_to_unicode(keyval), ibus_keyval_name(keyval));
//    g_object_unref(os);
//    g_object_unref(file);

    if (!ibus_keyval_to_unicode(keyval))
	    return FALSE;

    if (ibus_enchant_engine_process_key_event_node(enchant, keyval))
        return TRUE;

    gboolean handled = enchant->cur_node != enchant->root && keyval == IBUS_KEY_space;

    enchant->cur_node = enchant->root;
    ibus_enchant_engine_commit_preedit(enchant);

    if (handled)
        return TRUE;

    if (ibus_enchant_engine_process_key_event_node(enchant, keyval))
        return TRUE;

    return FALSE;
}
