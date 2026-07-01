digraph "main" {
  node [shape=box, style="filled,rounded", fontname="Courier New", fontsize=10];
  edge [fontname="Courier New", fontsize=9];

  0 [label="Block 0\nENTRY\ndebug_mode = 0\na = 10\nb = 20\nunused_calc = ((a * b) * 50)\ndecl user_input\nscanf(\"%99s\", user_input)\nIF_COND: debug_mode\n", fillcolor="#d4edda", color="#28a745"];
  0 -> 2 [label="True", color="#28a745", fontcolor="#28a745"];
  0 -> 3 [label="False", color="#dc3545", fontcolor="#dc3545"];
  1 [label="Block 1\nEXIT\n", fillcolor="#f8d7da", color="#dc3545"];
  2 [label="Block 2\nprintf(\"This branch is unreachable and will be Erased structually!\")\na = 999\n", fillcolor="#e2e3e5", color="#6c757d"];
  2 -> 4;
  3 [label="Block 3\nb = (a + b)\n", fillcolor="#e2e3e5", color="#6c757d"];
  3 -> 4;
  4 [label="Block 4\nsystem(user_input)\nreturn b\n", fillcolor="#e2e3e5", color="#6c757d"];
  4 -> 1;
  5 [label="Block 5\n", fillcolor="#e2e3e5", color="#6c757d"];
  5 -> 1;
}
