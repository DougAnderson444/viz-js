use anyhow::Context;
use std::{env, fs};

use wasmtime::{
    Config, Engine, Result, Store,
    component::{Component, Linker, bindgen},
};
use wasmtime_wasi::{self, ResourceTable, WasiCtx, WasiCtxView, WasiView};

use crate::exports::viz::component::viz_api::Object;

//
// NOTE:
//
// This example demonstrates how to call a component that exports the `viz-api`
// world you pasted (examples/component/viz.wit). It follows the same shape as
// the existing examples/component/main.rs in this repo but replaces the
// convert example with a `viz` example.
//
// The bindgen! invocation generates the Rust adapter types from the wit file.
// Put your viz.wit next to this file at ./examples/component/viz.wit (or adjust
// the path below) so the macro can read it at build time.
//
// The guest component (the C-built wasm) must be converted to a component
// (using the helper below) if it isn't already a component. For testing you
// can reuse the convert_to_component helper which wraps a module in a
// component. In production you'd normally build a component directly.
//
// The generated API names below (e.g. Viz, call_get_graphviz_version) follow
// the same convention as the other example in this repo. The exact generated
// names depend on the bindgen implementation and the WIT identifiers; if your
// generated API names differ, adjust the calls accordingly.
//

// Generate bindings of the guest component using the viz.wit file.
bindgen!("viz" in "../wit/world.wit");

struct MyState {
    wasi: WasiCtx,
    table: ResourceTable,
}

impl WasiView for MyState {
    fn ctx(&mut self) -> WasiCtxView<'_> {
        WasiCtxView {
            ctx: &mut self.wasi,
            table: &mut self.table,
        }
    }
}

fn main() -> Result<()> {
    // Allow overriding the guest wasm path from the command line for convenience.
    // Default matches the example layout; change to point at your compiled C wasm.
    let component_path = env::args()
        .nth(1)
        .unwrap_or_else(|| "../viz.wasm".to_string()); // Use viz.wasm as the default

    // Create an engine with the component model enabled.
    let mut config = Config::new();
    config.wasm_component_model(true); // enable component model
    let engine = Engine::new(&config)?;

    // Convert the raw wasm module to a component if needed (helper above).
    // If you already have a component binary, you can read it directly instead.
    let component_bytes = fs::read(&component_path).context("failed to read component file")?;

    // Instantiate the component.
    let component = Component::from_binary(&engine, &component_bytes)?;

    let mut linker: Linker<MyState> = Linker::new(&engine);

    // add the preview2 (p2) synchronous bindings
    wasmtime_wasi::p2::add_to_linker_sync(&mut linker)?;

    let wasi = WasiCtx::builder().inherit_stdio().inherit_args().build();
    let state = MyState {
        wasi,
        table: ResourceTable::new(),
    };
    // This example doesn't need any host-provided interfaces, so we use an empty
    // store state. If your host exposes callbacks to the guest, replace `()` with
    // your host state and add host::add_to_linker(...) as in the other example.
    let mut store = Store::new(&engine, state);

    // Instantiate the generated component adapter. The generated adapter type
    // name is derived from the world/name in the wit file; here we expect `Viz`.
    // If bindgen produced a different type name, adjust it.
    let viz = Viz::instantiate(&mut store, &component, &linker)?;

    // Many bindgen outputs put the guest methods under fields like `interface0`.
    // If your compiler tells you to use `viz.interface0` (as in your error output), use that field.
    // The calls below use `viz.interface0` consistently.
    let iface = &viz.interface0;

    // 1) Get Graphviz version
    let version = iface.call_get_graphviz_version(&mut store)?;
    println!("Graphviz version: {}", version);

    // 2) Get plugin list for "layout"
    let plugins = iface.call_get_plugin_list(&mut store, "layout")?;
    println!("Layout plugins: {:?}", plugins);

    // 3) Create a context and a graph
    let ctx = iface.call_create_context(&mut store)?;
    let graph = iface.call_create_graph(&mut store, "example", true, false)?;

    // 4) Add nodes and an edge
    let n1 = iface.call_add_node(&mut store, graph, "a")?;
    let _n2 = iface.call_add_node(&mut store, graph, "b")?;
    let _e = iface.call_add_edge(&mut store, graph, "a", "b")?;

    // 5) Set attributes (default node attribute)
    iface.call_set_default_node_attribute(&mut store, graph, "color", "red")?;

    // 6) set_attribute expects the `object` variant (graph/node/edge).
    // The exact name of the generated enum/variant constructors can differ depending on bindgen versions.
    // Common possibilities (pick whichever your compiler shows):
    //
    // - viz::object::Object::Node(n1)   <-- CamelCase enum variant under `object::Object`
    // - viz::object::node(n1)           <-- function-like constructor
    //
    // Try the first form; if the compiler suggests the other form use that. If you're unsure,
    // look at the generated bindings (search your crate for `mod viz` or the file generated by bindgen).
    let obj = Object::Node(n1);
    iface.call_set_attribute(&mut store, obj, "label", "A")?;

    // 7) Layout using "dot"
    match iface.call_layout(&mut store, ctx, graph, "dot") {
        Ok(Ok(())) => println!("Layout succeeded"),
        Ok(Err(err_str)) => println!("Layout failed: {}", err_str),
        Err(e) => println!("Layout error: {:?}", e),
    }

    // 8) Render to SVG
    match iface.call_render(&mut store, ctx, graph, "svg") {
        Ok(Ok(svg)) => {
            println!("Rendered SVG ({} bytes)", svg.len());
            println!("{}", &svg.as_str()[..std::cmp::min(2048, svg.len())]);
        }
        Ok(Err(err_str)) => println!("Render failed: {}", err_str),
        Err(e) => println!("Render error: {:?}", e),
    }

    // 9) Free layout resources
    iface.call_free_layout(&mut store, ctx, graph)?;

    Ok(())
}
