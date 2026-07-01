import streamlit as st
import subprocess
import os
import re
import traceback

st.set_page_config(page_title="C Code to Optimized CFG (C++)", layout="wide", page_icon="🔧")

# ─── Custom CSS for polish ─────────────────────────────────────────
st.markdown("""
<style>
    .stApp { }
    .block-container { padding-top: 1rem; }
    div[data-testid="stMetricValue"] { font-size: 1.4rem; }
    .stAlert p { font-size: 0.9rem; }
</style>
""", unsafe_allow_html=True)

st.title("🔧 C Code to Optimized CFG")
st.markdown("Paste C code or load a test case → see the **original** and **optimized** Control Flow Graph side by side.  \n*(Powered by C++/LLVM/Clang backend)*")

# ─── Sidebar: Test Case Loader ──────────────────────────────────────
st.sidebar.header("📂 Load Test Case")
# Check both the parent project test_cases/ and cpp_impl/test_cases/
cpp_dir = os.path.dirname(os.path.abspath(__file__))
parent_test_dir = os.path.join(os.path.dirname(cpp_dir), "test_cases")
local_test_dir = os.path.join(cpp_dir, "test_cases")
test_files = []
for td in [local_test_dir, parent_test_dir]:
    if os.path.exists(td):
        test_files += sorted([f for f in os.listdir(td) if f.endswith('.c')])

selected_test = st.sidebar.selectbox(
    "Select a test case file",
    ["— Use editor below —"] + test_files,
    index=0
)

# File uploader
st.sidebar.markdown("---")
st.sidebar.header("📤 Upload C File")
uploaded_file = st.sidebar.file_uploader("Upload a .c file", type=['c', 'h'])

# ─── Determine which code to use ────────────────────────────────────
DEFAULT_C_CODE = """#include <stdio.h>
#include <stdlib.h>

int main() {
    int debug_mode = 0;
    int a = 10;
    int b = 20;
    
    // Constant propagation & folding target
    int unused_calc = a * b * 50; 
    
    // Taint analysis target
    char user_input[100];
    scanf("%99s", user_input); 
    
    /* 
     * Because debug_mode = 0 above, the optimizer will propagate '0' into this condition.
     * The Branch Pruner will snip the 'True' Path edge.
     * The Unreachable Code pass will then permanently delete the printf block out of the Optimized CFG!
     */
    if (debug_mode) {
        printf("This branch is unreachable and will be Erased structually!");
        a = 999;
    } else {
        b = a + b;
    }
    
    // Security Sink Warning
    system(user_input);
    
    return b;
}
"""

initial_code = DEFAULT_C_CODE
if uploaded_file is not None:
    initial_code = uploaded_file.read().decode('utf-8')
elif selected_test and selected_test != "— Use editor below —":
    # Search both test directories for the selected file
    test_path = None
    for td in [local_test_dir, parent_test_dir]:
        candidate = os.path.join(td, selected_test)
        if os.path.exists(candidate):
            test_path = candidate
            break
    if test_path:
        with open(test_path, 'r') as f:
            initial_code = f.read()

# ─── Main Layout ────────────────────────────────────────────────────
col1, col2 = st.columns([1, 1])

with col1:
    st.subheader("📝 Input C Code")
    c_code = st.text_area("C Source Code", value=initial_code, height=500, label_visibility="collapsed")
    analyze_btn = st.button("🚀 Generate CFG & Optimize", type="primary", use_container_width=True)


# ─── Parser for C++ stdout ──────────────────────────────────────────

def parse_cpp_output(output):
    """Parses the C++ stdout into structured data per function,
    matching all sections the Python dashboard displays."""
    functions = {}
    current_func = None

    if output is None:
        output = ""

    lines = output.split('\n')
    for i, line in enumerate(lines):
        # Detect function
        m = re.search(r'Processing function:\s+(\w+)', line)
        if m:
            current_func = m.group(1)
            functions[current_func] = {
                'stats_before': {}, 'stats_after': {},
                'optimizations': [],
                'loops': [],
                'live_vars_in': {},
                'live_vars_out': {},
                'reaching_defs_in': {},
                'reaching_defs_out': {},
                'uninit_warnings': [],
            }
            continue

        if not current_func:
            continue

        data = functions[current_func]

        # Parse Stats Before
        sm = re.search(r'Before:\s+(\d+)\s+blocks,\s+(\d+)\s+edges,\s+(\d+)\s+stmts', line)
        if sm:
            data['stats_before'] = {
                'blocks': int(sm.group(1)),
                'edges': int(sm.group(2)),
                'statements': int(sm.group(3))
            }
            continue

        # Parse Stats After
        sm = re.search(r'After:\s+(\d+)\s+blocks,\s+(\d+)\s+edges,\s+(\d+)\s+stmts', line)
        if sm:
            data['stats_after'] = {
                'blocks': int(sm.group(1)),
                'edges': int(sm.group(2)),
                'statements': int(sm.group(3))
            }
            continue

        # Parse Reaching Definitions
        rdm = re.search(r'RD Block (\d+) IN: \{(.*?)\} OUT: \{(.*?)\}', line)
        if rdm:
            blk_id = rdm.group(1)
            data['reaching_defs_in'][blk_id] = rdm.group(2).strip()
            data['reaching_defs_out'][blk_id] = rdm.group(3).strip()
            continue

        # Parse Live Variables (IN + OUT)
        lvm = re.search(r'LV Block (\d+) IN: \{(.*?)\} OUT: \{(.*?)\}', line)
        if lvm:
            blk_id = lvm.group(1)
            data['live_vars_in'][blk_id] = lvm.group(2).strip()
            data['live_vars_out'][blk_id] = lvm.group(3).strip()
            continue


        # Parse Uninitialized Variable Warnings
        uninit_m = re.search(r"UNINIT_WARN var='(\w+)' in Block (\d+) stmt (\d+) context: (.+)", line)
        if uninit_m:
            data['uninit_warnings'].append({
                'var': uninit_m.group(1),
                'block': uninit_m.group(2),
                'stmt': uninit_m.group(3),
                'context': uninit_m.group(4).strip()
            })
            continue


    return functions


if analyze_btn:
    with col2:
        st.subheader("📊 Analysis Results")
        try:
            cpp_dir = os.path.dirname(os.path.abspath(__file__))
            temp_file = os.path.join(cpp_dir, "temp_input.c")
            with open(temp_file, "w", encoding="utf-8") as f:
                f.write(c_code)

            with st.spinner("Running C++ LLVM/Clang backend..."):
                p = subprocess.Popen(
                    ["wsl", "./build/cfg_tool", "temp_input.c", "--"],
                    cwd=cpp_dir,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    encoding='utf-8',
                    errors='replace'
                )
                stdout_data, stderr_data = p.communicate()
                returncode = p.returncode

            if returncode != 0:
                st.error("C++ backend failed to execute.")
                if stderr_data:
                    st.code(stderr_data)
                elif stdout_data:
                    st.code(stdout_data)
            else:
                if stderr_data and "error:" in stderr_data:
                    st.warning("Compilation warnings/errors detected:")
                    st.code(stderr_data)
                else:
                    st.success("✅ Parsing complete")

                parsed_data = parse_cpp_output(stdout_data)

                if not parsed_data:
                    st.warning("No functions found in the C code.")
                    st.code(stdout_data if stdout_data else "No stdout produced")

                for func_name, data in parsed_data.items():
                    st.markdown(f"---")
                    st.markdown(f"### 🔹 Function: `{func_name}`")

                    # ── Original CFG ────────────────────────────────────
                    before_dot_path = os.path.join(cpp_dir, f"{func_name}_before.dot")
                    with st.expander("📐 Original CFG", expanded=True):
                        if os.path.exists(before_dot_path):
                            with open(before_dot_path, "r") as f:
                                st.graphviz_chart(f.read())
                        else:
                            st.info("No original CFG DOT file found.")

                   
                    
                    # ── Reaching Definitions ────────────────────────────
                    with st.expander("📍 Reaching Definitions Analysis"):
                        if data['reaching_defs_in']:
                            for blk_id in sorted(data['reaching_defs_in'].keys(), key=int):
                                in_defs = data['reaching_defs_in'][blk_id]
                                out_defs = data['reaching_defs_out'].get(blk_id, '')
                                st.markdown(f"**Block {blk_id}**")
                                in_display = f"`[{in_defs}]`" if in_defs else "`∅`"
                                out_display = f"`[{out_defs}]`" if out_defs else "`∅`"
                                st.markdown(f"- IN: {in_display}")
                                st.markdown(f"- OUT: {out_display}")
                        else:
                            st.write("No reaching definitions data.")

                    # ── Live Variable Analysis ──────────────────────────
                    with st.expander("💓 Live Variable Analysis"):
                        if data['live_vars_in']:
                            for blk_id in sorted(data['live_vars_in'].keys(), key=int):
                                lv_in = data['live_vars_in'][blk_id]
                                lv_out = data['live_vars_out'].get(blk_id, '')
                                st.markdown(f"**Block {blk_id}**")
                                in_display = f"`{{{lv_in}}}`" if lv_in else "`∅`"
                                out_display = f"`{{{lv_out}}}`" if lv_out else "`∅`"
                                st.markdown(f"- IN (live): {in_display}")
                                st.markdown(f"- OUT (live): {out_display}")
                        else:
                            st.write("No live variable data.")

                    # ── Uninitialized Variable Warnings ──────────────────
                    with st.expander("⚠️ Uninitialized Variable Detection"):
                        if data['uninit_warnings']:
                            st.warning(f"**{len(data['uninit_warnings'])} potential uninitialized variable use(s) detected**")
                            for w in data['uninit_warnings']:
                                st.markdown(f"- Variable `{w['var']}` used in Block `{w['block']}` (stmt {w['stmt']}): `{w['context']}`")
                        else:
                            st.success("No uninitialized variable warnings.")

                    # ── Optimization Passes ─────────────────────────────
                    st.markdown("#### ⚡ Optimization Passes")

                    # Show statistics
                    if data['stats_before'] and data['stats_after']:
                        pre = data['stats_before']
                        post = data['stats_after']
                        m1, m2, m3, m4 = st.columns(4)
                        m1.metric("Blocks (before)", pre['blocks'])
                        m2.metric("Blocks (after)", post['blocks'],
                                  delta=post['blocks'] - pre['blocks'])
                        m3.metric("Statements (before)", pre['statements'])
                        m4.metric("Statements (after)", post['statements'],
                                  delta=post['statements'] - pre['statements'])

                    # Show optimizer log
                    if data['optimizations']:
                        with st.expander("📋 Optimization Log", expanded=True):
                            for entry in data['optimizations']:
                                st.markdown(f"- {entry}")
                    else:
                        st.info("No optimizations could be applied.")

                    # ── Optimized CFG ───────────────────────────────────
                    after_dot_path = os.path.join(cpp_dir, f"{func_name}_after.dot")
                    with st.expander("📐 Optimized CFG", expanded=True):
                        if os.path.exists(after_dot_path):
                            with open(after_dot_path, "r") as f:
                                optimized_dot = f.read()
                            st.graphviz_chart(optimized_dot)
                        else:
                            st.info("No optimized CFG DOT file found.")
                            optimized_dot = ""

                    # ── Download DOT ────────────────────────────────────
                    if os.path.exists(after_dot_path):
                        with open(after_dot_path, "r") as f:
                            dl_dot = f.read()
                        st.download_button(
                            label=f"⬇️ Download Optimized DOT ({func_name})",
                            data=dl_dot,
                            file_name=f"{func_name}_optimized.dot",
                            mime="text/plain",
                            use_container_width=True,
                            key=f"dl_opt_{func_name}"
                        )

                # Show raw C++ output log at the bottom
                with st.expander("📋 C++ Raw Output Log"):
                    st.text(stdout_data)

        except Exception as e:
            st.error("An error occurred during analysis.")
            st.code(traceback.format_exc())