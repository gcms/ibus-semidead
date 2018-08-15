/* vim:set et sts=4: */

#include <xkbcommon/xkbcommon.h>
#include "engine.h"

void ibus_semidead_log(const gchar *format, ...) {
#ifdef __DEBUG__
    va_list args;
    va_start (args, format);

    GFile *file = g_file_new_for_path ("/tmp/keyval");
    GOutputStream *os = g_file_append_to(file, G_FILE_CREATE_NONE,
      NULL, NULL);
    g_output_stream_vprintf(os, NULL, NULL, NULL, format, args);
    g_output_stream_flush(os, NULL, NULL);
    g_object_unref(os);
    g_object_unref(file);

    va_end(args);
#endif
}

typedef struct _IBusSemiDeadEngine IBusSemiDeadEngine;
typedef struct _IBusSemiDeadEngineClass IBusSemiDeadEngineClass;

struct _IBusSemiDeadEngine {
    IBusEngineSimple parent;

    /* members */
    gboolean enabled;
    GString *preedit;

    GNode *root;
    GNode *cur_node;
};

struct _IBusSemiDeadEngineClass {
    IBusEngineSimpleClass parent;
};

/* functions prototype */
static void	ibus_semidead_engine_class_init	(IBusSemiDeadEngineClass	*klass);
static void	ibus_semidead_engine_init		(IBusSemiDeadEngine		*engine);
static void	ibus_semidead_engine_destroy		(IBusSemiDeadEngine		*engine);

static gboolean
ibus_semidead_engine_process_key_event
        (IBusEngine            *engine,
         guint               	 keyval,
         guint               	 keycode,
         guint               	 modifiers);

#define ibus_semidead_engine_in_sequence(e) ((e)->cur_node != (e)->root)

// #define DEFINE_FUNC_EVENT(name) \
// static void ibus_semidead_engine_##name (IBusEngine *engine) { \
//   ibus_semidead_debug("event: %s\n", #name); \
// }
//
//
//
// //#define ibus_semidead_engine_focus_out ibus_semidead_engine_clear_sequence
// //#define ibus_semidead_engine_reset ibus_semidead_engine_clear_sequence
//
// DEFINE_FUNC_EVENT(reset)
// DEFINE_FUNC_EVENT(focus_out)
//
// DEFINE_FUNC_EVENT(focus_in)
//
// DEFINE_FUNC_EVENT(disable)
// DEFINE_FUNC_EVENT(page_up)
// DEFINE_FUNC_EVENT(page_down)
//
// DEFINE_FUNC_EVENT(cursor_up)
// DEFINE_FUNC_EVENT(cursor_down)
//
// DEFINE_FUNC_EVENT(property_activate)
// DEFINE_FUNC_EVENT(property_show)
// DEFINE_FUNC_EVENT(property_hide)
//
// DEFINE_FUNC_EVENT(process_hand_writing_event)
// DEFINE_FUNC_EVENT(cancel_hand_writing)
// DEFINE_FUNC_EVENT(set_content_type)
//

static void
ibus_semidead_engine_enable (IBusEngine *engine);


static void
ibus_semidead_engine_set_capabilities (IBusEngine *engine,
                                        guint       caps);
static void
ibus_semidead_engine_candidate_clicked (IBusEngine *engine,
                               guint       index,
                               guint       button,
                               guint       state)
{
    ibus_semidead_debug("candidate-clicked\n");
}

static void
ibus_semidead_engine_cursor_location (IBusEngine *sdengine,
                                    gint             x,
                                    gint             y,
                                    gint             w,
                                    gint             h) {
  ibus_semidead_debug ("cursor_location in_sequence %s\n", ibus_semidead_engine_in_sequence((IBusSemiDeadEngine *) sdengine) ? "true" : "false");
}

static void
ibus_semidead_engine_set_surrounding_text (IBusEngine *sdengine,
                                  IBusText   *text,
                                  guint       cursor_pos,
                                  guint       anchor_pos)
{
    g_assert (IBUS_IS_ENGINE (sdengine));

    ibus_semidead_debug ("cl ibus_engine_set_surrounding_text %s\n", ibus_semidead_engine_in_sequence((IBusSemiDeadEngine *) sdengine) ? "true" : "false");
}

static void
ibus_semidead_engine_commit_string
        (IBusSemiDeadEngine      *sdengine,
         const gchar            *string);

static gboolean
ibus_semidead_engine_match_keyval
        (IBusSemiDeadEngine *sdengine,
         guint keyval);

static void
ibus_semidead_engine_commit(IBusEngine *engine);

static void
ibus_semidead_engine_cancel(IBusEngine *engine);

G_DEFINE_TYPE (IBusSemiDeadEngine, ibus_semidead_engine, IBUS_TYPE_ENGINE_SIMPLE)

static void
ibus_semidead_engine_class_init (IBusSemiDeadEngineClass *klass)
{
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS (klass);
    ibus_object_class->destroy = (IBusObjectDestroyFunc) ibus_semidead_engine_destroy;

    IBusEngineClass *engine_class = IBUS_ENGINE_CLASS (klass);
    engine_class->process_key_event = ibus_semidead_engine_process_key_event;

    engine_class->focus_out = ibus_semidead_engine_cancel;
    engine_class->reset = ibus_semidead_engine_cancel;


    engine_class->set_cursor_location = ibus_semidead_engine_cursor_location;
    engine_class->set_surrounding_text = ibus_semidead_engine_set_surrounding_text;
    engine_class->set_capabilities = ibus_semidead_engine_set_capabilities;
    engine_class->candidate_clicked = ibus_semidead_engine_candidate_clicked;
    engine_class->focus_in = ibus_semidead_engine_focus_in;
    engine_class->enable = ibus_semidead_engine_enable;
    engine_class->disable = ibus_semidead_engine_disable;
    engine_class->page_up = ibus_semidead_engine_page_up;
    engine_class->page_down = ibus_semidead_engine_page_down;

    engine_class->cursor_up = ibus_semidead_engine_cursor_up;
    engine_class->cursor_down = ibus_semidead_engine_cursor_down;
    engine_class->property_activate = ibus_semidead_engine_property_activate;
    engine_class->property_show = ibus_semidead_engine_property_show;
    engine_class->property_hide = ibus_semidead_engine_property_hide;

    engine_class->process_hand_writing_event = ibus_semidead_engine_process_hand_writing_event;
    engine_class->cancel_hand_writing = ibus_semidead_engine_cancel_hand_writing;
    engine_class->set_content_type = ibus_semidead_engine_set_content_type;

  }

static void
ibus_semidead_engine_init_tree(GNode *root,
                               const gchar *keyval_name,
                               const gchar *from,
                               const gchar *to) {
    glong len = strlen(from);
    g_return_if_fail(len == g_utf8_strlen(to, strlen(to)));

    guint keyval = ibus_keyval_from_name(keyval_name);
    g_return_if_fail(keyval);

    GNode *symbol_node = g_node_append_data(root, keyval);

    gchar* vowel_keyval_name[2];
    vowel_keyval_name[1] = NULL;
    while (*from) {
        *vowel_keyval_name = *from;

        ibus_semidead_debug("add to %s -> %s (%c)\n", keyval_name, vowel_keyval_name, ibus_keyval_from_name(vowel_keyval_name));
        GNode *vowel_node = g_node_append_data(symbol_node, ibus_keyval_from_name(vowel_keyval_name));
        g_node_append_data(vowel_node, g_utf8_get_char(to));

        from++;
        to = g_utf8_next_char(to);
    }
}

static void
ibus_semidead_engine_init (IBusSemiDeadEngine *sdengine)
{
    sdengine->enabled = TRUE;

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


//    sdengine->simple = ibus_engine_new_with_type (IBUS_TYPE_ENGINE_SIMPLE,
//                                                      "xkb:us:intl:eng",
//                                                      "/org/freedesktop/IBus/Engine/simple/898",
//                                                      ibus_bus_get_connection ((IBusService *) sdengine));

}

static void
ibus_semidead_engine_enable (IBusEngine *engine)
{
   ibus_semidead_debug("event: enable\tenabled? %d\n", engine->enabled);

   engine->enabled = TRUE;
}

static void
ibus_semidead_engine_set_capabilities (IBusEngine *engine,
                                        guint       caps)
{
  ibus_semidead_debug ("set-capabilities (0x%04x)%d", caps, (caps & IBUS_CAP_PREEDIT_TEXT));

  ((IBusSemiDeadEngine*) engine)->enabled = caps & IBUS_CAP_PREEDIT_TEXT;
}


static void
ibus_semidead_engine_destroy (IBusSemiDeadEngine *sdengine)
{
    if (sdengine->preedit) {
        g_string_free (sdengine->preedit, TRUE);
        sdengine->preedit = NULL;
    }

    if (sdengine->root) {
        g_node_destroy(sdengine->root);
        sdengine->root = NULL;
    }

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

    ibus_engine_update_preedit_text_with_mode ((IBusEngine *)sdengine, text, preedit->len, TRUE, IBUS_ENGINE_PREEDIT_CLEAR);

}

static void
ibus_semidead_engine_update_preedit (IBusSemiDeadEngine *sdengine)
{
    ibus_semidead_engine_update_preedit_text (sdengine, sdengine->preedit);
}

static void
ibus_semidead_engine_clean_preedit (IBusSemiDeadEngine *sdengine) {
    sdengine->cur_node = sdengine->root;
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

    sdengine->cur_node = sdengine->root;

    return TRUE;
}

static void
ibus_semidead_engine_commit(IBusEngine *engine) {
  ibus_semidead_engine_commit_preedit((IBusSemiDeadEngine*) engine);
}

static void
ibus_semidead_engine_cancel(IBusEngine *engine) {
  ibus_semidead_engine_clean_preedit((IBusSemiDeadEngine *) engine);
}


static gboolean
ibus_semidead_engine_match_keyval (IBusSemiDeadEngine *sdengine,
                                   guint keyval) {
/*
    ibus_semidead_debug("IS ROOT %d\n", sdengine->cur_node == sdengine->root);
    ibus_semidead_debug("N CHILDREN: %d\n", g_node_n_children(sdengine->cur_node));

    GNode *child = g_node_first_child(sdengine->cur_node);
    int i = 0;
    while (child) {
        ibus_semidead_debug ("CHILD %d = %s\r\n", ++i, child ? ibus_keyval_name(child->data) : "");
        child = g_node_next_sibling(child);
    }
*/
    GNode *node = g_node_find_child(sdengine->cur_node, G_TRAVERSE_NON_LEAVES, keyval);

//    ibus_semidead_debug ("NODE = %s\r\n", node ? ibus_keyval_name(keyval) : "");
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

/* copied from ibus source code */
static gboolean
is_modifier (guint keyval)
{

//    IBUS_COMPOSE_IGNORE_KEYLIST
    switch(keyval) {
    case IBUS_KEY_Shift_L:
    case IBUS_KEY_Shift_R:
    case IBUS_KEY_Shift_Lock:
    case IBUS_KEY_Caps_Lock:
    case IBUS_KEY_Control_L:
    case IBUS_KEY_Control_R:
    case IBUS_KEY_Alt_L:
    case IBUS_KEY_Alt_R:
    case IBUS_KEY_Meta_L:
    case IBUS_KEY_Num_Lock:
    case IBUS_KEY_Super_L:
    case IBUS_KEY_Hyper_L:
    case IBUS_KEY_ISO_Level3_Shift:
    case IBUS_KEY_Mode_switch:
        return TRUE;
    default:
        return FALSE;
    }
}

static gboolean
is_cancel(guint keyval) {
    switch(keyval) {
        case IBUS_KEY_BackSpace:
        case IBUS_KEY_Escape:
        case IBUS_KEY_Delete:
            return TRUE;
        default:
            return FALSE;
    }
}

#define ibus_semidead_engine_enabled(e) (((IBusEngine*)(e))->enabled && (e)->enabled)

static gboolean
do_ibus_semidead_engine_process_key_event (IBusEngine *engine,
                                       guint       keyval,
                                       guint       keycode,
                                       guint       modifiers)
{
    IBusSemiDeadEngine *sdengine = (IBusSemiDeadEngine *)engine;

    gboolean in_sequence = ibus_semidead_engine_in_sequence(sdengine);

    // IGNORE KEY RELEASE
    if (modifiers & IBUS_RELEASE_MASK)
        return FALSE;

    if (is_modifier(keyval))
        return FALSE;

//    if (TRUE)
//        return FALSE;

    ibus_semidead_debug ("process_key_event %s %d %d %d %s\n", in_sequence ? "true" : "false", keyval, keycode, modifiers, ibus_keyval_name(keyval));

    // INTERRUPT SEQUENCE
    if (modifiers & (IBUS_CONTROL_MASK | IBUS_MOD1_MASK)) {
//    if (modifiers & ~IBUS_SHIFT_MASK) {
        if (in_sequence && ibus_keyval_to_unicode(keyval))
            ibus_semidead_engine_commit_preedit(sdengine);

        return FALSE;
    }

    // FINISH SEQUENCE
    if (in_sequence && keyval == IBUS_KEY_space) {
        ibus_semidead_engine_commit_preedit(sdengine);
        return TRUE;
    }

    // CANCEL SEQUENCE
    if (in_sequence && is_cancel(keyval)) {
        ibus_semidead_engine_clean_preedit(sdengine);
        return TRUE;
    }

    // MATCH SEQUENCE
    if (ibus_semidead_engine_match_keyval(sdengine, keyval))
        return TRUE;

    // INTERRUPT SEQUENCE
    if (in_sequence)
        ibus_semidead_engine_commit_preedit(sdengine);

    // TRY TO START A NEW SEQUENCE
    return ibus_semidead_engine_match_keyval(sdengine, keyval);
}

//static gboolean
//ibus_semidead_engine_process_key_event (IBusEngine *engine,
//                                        guint       keyval,
//                                        guint       keycode,
//                                        guint       modifiers) {
//    IBusSemiDeadEngine *sdengine = (IBusSemiDeadEngine *)engine;
//
//    if (!do_ibus_semidead_engine_process_key_event(engine, keyval, keycode, modifiers))
//        return IBUS_ENGINE_GET_CLASS(sdengine->simple)->process_key_event(engine, keyval, keycode, modifiers);
//}

static gboolean
ibus_semidead_engine_process_key_event (IBusEngine *engine,
                                       guint       keyval,
                                       guint       keycode,
                                       guint       modifiers) {

    ibus_semidead_debug("cursor: %d,%d / %d,%d\n",
        engine->cursor_area.x,
        engine->cursor_area.y,
        engine->cursor_area.width,
        engine->cursor_area.height);

    if (!ibus_semidead_engine_enabled((IBusSemiDeadEngine*) engine))
        return FALSE;

    ibus_semidead_debug("process_key_event %0d %0d %0x\n", keyval, keycode, modifiers);
    gboolean result = do_ibus_semidead_engine_process_key_event(engine, keyval, keycode, modifiers);
    ibus_semidead_debug("key_handled: %d\n", result);

                                         //ibus_engine_forward_key_event(engine, keyval, keycode, state)
    return result;
}


static void
ibus_semidead_engine_clear_sequence
        (IBusEngine             *sdengine) {
    ibus_semidead_debug("clear_sequence\n");
    if (ibus_semidead_engine_in_sequence((IBusSemiDeadEngine *) sdengine)) {
        ibus_semidead_debug("in_sequence\n");
        ibus_semidead_engine_clean_preedit((IBusSemiDeadEngine *) sdengine);
    }

    ibus_semidead_debug ("in_sequence %s\n", ibus_semidead_engine_in_sequence((IBusSemiDeadEngine *) sdengine) ? "true" : "false");
}
