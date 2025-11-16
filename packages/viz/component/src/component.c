// packages/viz/component/src/component.c

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Graphviz headers
#include <gvc.h>
#include <cgraph.h>

// wit-bindgen generated header
#include "viz.h"

// --- Global Variables ---
// These are extern in viz.c, so we define them here.
extern int Y_invert;
extern unsigned char Reduce;

// For Graphviz plugin loading
extern gvplugin_library_t gvplugin_core_LTX_library;
extern gvplugin_library_t gvplugin_dot_layout_LTX_library;
extern gvplugin_library_t gvplugin_neato_layout_LTX_library;

lt_symlist_t lt_preloaded_symbols[] = {
  { "gvplugin_core_LTX_library", &gvplugin_core_LTX_library},
  { "gvplugin_dot_layout_LTX_library", &gvplugin_dot_layout_LTX_library},
  { "gvplugin_neato_layout_LTX_library", &gvplugin_neato_layout_LTX_library},
  { 0, 0 }
};

// --- Error Handling ---
#define MAX_ERROR_MESSAGE_LENGTH 1024
static char g_error_messages[MAX_ERROR_MESSAGE_LENGTH];
static int g_error_len = 0;

static int viz_error_handler(char *text) {
    int len = strlen(text);
    if (g_error_len + len + 1 < MAX_ERROR_MESSAGE_LENGTH) { // +1 for newline
        if (g_error_len > 0) {
            strcat(g_error_messages, "\n");
            g_error_len++;
        }
        strcat(g_error_messages, text);
        g_error_len += len;
    }
    return 0; // Continue processing errors
}

static void viz_reset_errors() {
    g_error_messages[0] = '\0';
    g_error_len = 0;
    agseterrf(viz_error_handler);
    agseterr(AGWARN);
    agreseterrors();
}

// --- Helper Functions for String Conversion ---

// Converts a wit-bindgen string to a null-terminated C string.
// The caller is responsible for freeing the returned string.
static char* wit_string_to_c_string(viz_string_t *wit_str) {
    char* c_str = (char*)malloc(wit_str->len + 1);
    if (!c_str) return NULL;
    memcpy(c_str, wit_str->ptr, wit_str->len);
    c_str[wit_str->len] = '\0';
    return c_str;
}

// Converts a null-terminated C string to a wit-bindgen string.
// The wit_str will be allocated and populated.
static void c_string_to_wit_string(const char* c_str, viz_string_t *wit_str) {
    if (!c_str) {
        viz_string_set(wit_str, "");
        return;
    }
    size_t len = strlen(c_str);
    wit_str->ptr = (uint8_t*)malloc(len);
    if (!wit_str->ptr) {
        wit_str->len = 0;
        return;
    }
    memcpy(wit_str->ptr, c_str, len);
    wit_str->len = len;
}

// --- Resource Destructors ---

void exports_viz_component_viz_api_graph_destructor(exports_viz_component_viz_api_graph_t *rep) {
    if (rep) {
        agclose((Agraph_t*)rep);
    }
}

void exports_viz_component_viz_api_context_destructor(exports_viz_component_viz_api_context_t *rep) {
    if (rep) {
        gvFinalize((GVC_t*)rep);
        gvFreeContext((GVC_t*)rep);
    }
}

// Node and Edge are part of a graph, so they don't need explicit destructors
// as they are freed when the graph is closed.
// However, wit-bindgen generates destructors for all resources.
// We'll provide empty ones to satisfy the linker.
void exports_viz_component_viz_api_node_destructor(exports_viz_component_viz_api_node_t *rep) {
    (void)rep; // Unused parameter
}

void exports_viz_component_viz_api_edge_destructor(exports_viz_component_viz_api_edge_t *rep) {
    (void)rep; // Unused parameter
}


// --- WIT Exported Functions Implementation ---

void exports_viz_component_viz_api_set_y_invert(int32_t value) {
    Y_invert = value;
}

void exports_viz_component_viz_api_set_reduce(int32_t value) {
    Reduce = (unsigned char)value;
}

void exports_viz_component_viz_api_get_graphviz_version(viz_string_t *ret) {
    GVC_t *context = NULL;
    char *version_str = NULL;

    context = gvContextPlugins(lt_preloaded_symbols, 0);
    if (context) {
        version_str = gvcVersion(context);
        c_string_to_wit_string(version_str, ret);
        gvFinalize(context);
        gvFreeContext(context);
    } else {
        c_string_to_wit_string("Error: Could not create Graphviz context.", ret);
    }
}

void exports_viz_component_viz_api_get_plugin_list(viz_string_t *kind, viz_list_string_t *ret) {
    GVC_t *context = NULL;
    char **list = NULL;
    int count = 0;
    char* c_kind = wit_string_to_c_string(kind);

    context = gvContextPlugins(lt_preloaded_symbols, 0);
    if (context) {
        list = gvPluginList(context, c_kind, &count);
        
        ret->ptr = (viz_string_t*)malloc(count * sizeof(viz_string_t));
        ret->len = count;

        for (int i = 0; i < count; ++i) {
            c_string_to_wit_string(list[i], &ret->ptr[i]);
        }
        
        gvFinalize(context);
        gvFreeContext(context);
    } else {
        ret->ptr = NULL;
        ret->len = 0;
    }
    free(c_kind);
}

exports_viz_component_viz_api_own_graph_t exports_viz_component_viz_api_create_graph(viz_string_t *name, bool directed, bool strict) {
    char* c_name = wit_string_to_c_string(name);
    Agdesc_t desc = { .directed = directed, .strict = strict };
    Agraph_t *g = agopen(c_name, desc, NULL);
    free(c_name);
    return exports_viz_component_viz_api_graph_new((exports_viz_component_viz_api_graph_t*)g);
}

bool exports_viz_component_viz_api_read_one_graph(viz_string_t *dot_string, exports_viz_component_viz_api_own_graph_t *ret_ok, viz_string_t *ret_err) {
    viz_reset_errors();

    char* c_dot_string = wit_string_to_c_string(dot_string);

    // Workaround for #218. Set the global default node label.
    agattr(NULL, AGNODE, "label", "\\N");

    Agraph_t *graph = agmemread(c_dot_string);
    free(c_dot_string);

    // Consume the rest of the input
    Agraph_t *other_graph = NULL;
    do {
        other_graph = agmemread(NULL);
        if (other_graph) {
            agclose(other_graph);
        }
    } while (other_graph);

    if (g_error_len > 0) {
        if (graph) agclose(graph); // Close if partially read
        c_string_to_wit_string(g_error_messages, ret_err);
        return false; // Error
    }

    if (!graph) {
        c_string_to_wit_string("Failed to read graph: unknown error.", ret_err);
        return false; // Error
    }

    *ret_ok = exports_viz_component_viz_api_graph_new((exports_viz_component_viz_api_graph_t*)graph);
    return true; // Success
}

exports_viz_component_viz_api_own_node_t exports_viz_component_viz_api_add_node(exports_viz_component_viz_api_borrow_graph_t g, viz_string_t *name) {
    Agraph_t *c_g = (Agraph_t*)g;
    char* c_name = wit_string_to_c_string(name);
    Agnode_t *n = agnode(c_g, c_name, true);
    free(c_name);
    return exports_viz_component_viz_api_node_new((exports_viz_component_viz_api_node_t*)n);
}

exports_viz_component_viz_api_own_edge_t exports_viz_component_viz_api_add_edge(exports_viz_component_viz_api_borrow_graph_t g, viz_string_t *u_name, viz_string_t *v_name) {
    Agraph_t *c_g = (Agraph_t*)g;
    char* c_u_name = wit_string_to_c_string(u_name);
    char* c_v_name = wit_string_to_c_string(v_name);
    Agnode_t *u = agnode(c_g, c_u_name, true);
    Agnode_t *v = agnode(c_g, c_v_name, true);
    Agedge_t *e = agedge(c_g, u, v, NULL, true);
    free(c_u_name);
    free(c_v_name);
    return exports_viz_component_viz_api_edge_new((exports_viz_component_viz_api_edge_t*)e);
}

exports_viz_component_viz_api_own_graph_t exports_viz_component_viz_api_add_subgraph(exports_viz_component_viz_api_borrow_graph_t g, viz_string_t *name) {
    Agraph_t *c_g = (Agraph_t*)g;
    char* c_name = wit_string_to_c_string(name);
    Agraph_t *subg = agsubg(c_g, c_name, true);
    free(c_name);
    return exports_viz_component_viz_api_graph_new((exports_viz_component_viz_api_graph_t*)subg);
}

void exports_viz_component_viz_api_set_default_graph_attribute(exports_viz_component_viz_api_borrow_graph_t g, viz_string_t *name, viz_string_t *value) {
    Agraph_t *c_g = (Agraph_t*)g;
    char* c_name = wit_string_to_c_string(name);
    char* c_value = wit_string_to_c_string(value);
    agattr(c_g, AGRAPH, c_name, c_value);
    free(c_name);
    free(c_value);
}

void exports_viz_component_viz_api_set_default_node_attribute(exports_viz_component_viz_api_borrow_graph_t g, viz_string_t *name, viz_string_t *value) {
    Agraph_t *c_g = (Agraph_t*)g;
    char* c_name = wit_string_to_c_string(name);
    char* c_value = wit_string_to_c_string(value);
    agattr(c_g, AGNODE, c_name, c_value);
    free(c_name);
    free(c_value);
}

void exports_viz_component_viz_api_set_default_edge_attribute(exports_viz_component_viz_api_borrow_graph_t g, viz_string_t *name, viz_string_t *value) {
    Agraph_t *c_g = (Agraph_t*)g;
    char* c_name = wit_string_to_c_string(name);
    char* c_value = wit_string_to_c_string(value);
    agattr(c_g, AGEDGE, c_name, c_value);
    free(c_name);
    free(c_value);
}

void exports_viz_component_viz_api_set_attribute(exports_viz_component_viz_api_object_t *obj, viz_string_t *name, viz_string_t *value) {
    void* c_obj = NULL;
    switch (obj->tag) {
        case EXPORTS_VIZ_COMPONENT_VIZ_API_OBJECT_GRAPH:
            c_obj = exports_viz_component_viz_api_graph_rep(obj->val.graph);
            break;
        case EXPORTS_VIZ_COMPONENT_VIZ_API_OBJECT_NODE:
            c_obj = exports_viz_component_viz_api_node_rep(obj->val.node);
            break;
        case EXPORTS_VIZ_COMPONENT_VIZ_API_OBJECT_EDGE:
            c_obj = exports_viz_component_viz_api_edge_rep(obj->val.edge);
            break;
    }

    if (c_obj) {
        char* c_name = wit_string_to_c_string(name);
        char* c_value = wit_string_to_c_string(value);
        agsafeset(c_obj, c_name, c_value, "");
        free(c_name);
        free(c_value);
    }
}

exports_viz_component_viz_api_own_context_t exports_viz_component_viz_api_create_context() {
    GVC_t *context = gvContextPlugins(lt_preloaded_symbols, 0);
    return exports_viz_component_viz_api_context_new((exports_viz_component_viz_api_context_t*)context);
}

bool exports_viz_component_viz_api_layout(exports_viz_component_viz_api_borrow_context_t ctx, exports_viz_component_viz_api_borrow_graph_t g, viz_string_t *engine, viz_string_t *ret_err) {
    viz_reset_errors();
    GVC_t *c_ctx = (GVC_t*)ctx;
    Agraph_t *c_g = (Agraph_t*)g;
    char* c_engine = wit_string_to_c_string(engine);

    int layout_error = gvLayout(c_ctx, c_g, c_engine);
    free(c_engine);

    if (layout_error != 0 || g_error_len > 0) {
        c_string_to_wit_string(g_error_messages, ret_err);
        return false; // Error
    }
    return true; // Success
}

void exports_viz_component_viz_api_free_layout(exports_viz_component_viz_api_borrow_context_t ctx, exports_viz_component_viz_api_borrow_graph_t g) {
    GVC_t *c_ctx = (GVC_t*)ctx;
    Agraph_t *c_g = (Agraph_t*)g;
    gvFreeLayout(c_ctx, c_g);
}

bool exports_viz_component_viz_api_render(exports_viz_component_viz_api_borrow_context_t ctx, exports_viz_component_viz_api_borrow_graph_t g, viz_string_t *format, viz_string_t *ret_ok, viz_string_t *ret_err) {
    viz_reset_errors();
    GVC_t *c_ctx = (GVC_t*)ctx;
    Agraph_t *c_g = (Agraph_t*)g;
    char* c_format = wit_string_to_c_string(format);

    char *data = NULL;
    size_t length = 0;
    int render_error = gvRenderData(c_ctx, c_g, c_format, &data, &length);
    free(c_format);

    if (render_error != 0 || g_error_len > 0) {
        if (data) gvFreeRenderData(data);
        c_string_to_wit_string(g_error_messages, ret_err);
        return false; // Error
    }

    if (!data) {
        c_string_to_wit_string("Failed to render graph: no data returned.", ret_err);
        return false; // Error
    }

    // Convert rendered data to wit-bindgen string
    // Note: gvRenderData returns a null-terminated string for text formats like SVG.
    // If it were binary, we'd need to handle length differently.
    c_string_to_wit_string(data, ret_ok);
    gvFreeRenderData(data); // Free Graphviz allocated data

    return true; // Success
}
