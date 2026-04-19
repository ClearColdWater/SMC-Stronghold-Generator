# drafted with GPT-5.4

import json
import math
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib import colors
from matplotlib import ticker as mticker

HEATMAP_TYPE_ORDER = [
    "BRANCHABLE_CORRIDOR",
    "PRISON_CELL",
    "LEFT_TURN",
    "RIGHT_TURN",
    "ROOM_CROSSING",
    "STRAIGHT_STAIRS",
    "SPIRAL_STAIRS",
    "FIVE_WAY_CROSSING",
    "CHEST_CORRIDOR",
    "LIBRARY",
    "PORTAL_ROOM",
    "SMALL_CORRIDOR",
    "NO_REAL_PIECE",
]


def load_stats_report(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def get_observation_nodes(report):
    total_nodes = report["observation"]["tree"]["totalNodes"]
    return report["observation"]["tree"]["nodes"][:total_nodes]


def build_tree(nodes):
    n = len(nodes)
    children = {i: [] for i in range(n)}
    parent = [-1] * n
    edge_slot = {}

    for i, node in enumerate(nodes):
        ch = node["ch"]
        for slot, child in enumerate(ch):
            if child >= 0:
                children[i].append(child)
                parent[child] = i
                edge_slot[(i, child)] = slot

    roots = [i for i in range(n) if parent[i] == -1]
    return children, parent, roots, edge_slot


def compute_layout(children, roots):
    subtree_size = {}
    depth = {}

    def dfs_size(u, d):
        depth[u] = d
        if not children[u]:
            subtree_size[u] = 1
            return 1
        total = 0
        for v in children[u]:
            total += dfs_size(v, d + 1)
        subtree_size[u] = max(total, 1)
        return subtree_size[u]

    for r in roots:
        dfs_size(r, 0)

    pos = {}

    def dfs_pos(u, left, d):
        if not children[u]:
            x = left + 0.5
        else:
            cur = left
            child_x = []
            for v in children[u]:
                dfs_pos(v, cur, d + 1)
                child_x.append(pos[v][0])
                cur += subtree_size[v]
            x = sum(child_x) / len(child_x)
        pos[u] = (x, -d)

    cur_left = 0
    for r in roots:
        dfs_pos(r, cur_left, 0)
        cur_left += subtree_size[r] + 1

    max_depth = max((-y for _, y in pos.values()), default=0)
    max_width = max((x for x, _ in pos.values()), default=1)

    return pos, max_depth, max_width, depth


def get_metric_and_se(report, key):
    """
    estimates stderr with uniqueAncestors
    """
    N_total = report.get("uniqueAncestors", 0)
    if N_total <= 0:
        N_total = report.get("rawSampleCount", 1)
    if N_total <= 0:
        N_total = 1

    total_w = float(report.get("totalWeight", 0.0))
    if total_w <= 0:
        total_w = float(report.get("rawSampleCount", 1))
    if total_w <= 0:
        total_w = 1.0

    vals = []
    ses = []

    for node in report["nodes"]:
        v = node.get(key, None)
        val = np.nan if v is None else float(v)
        vals.append(val)

        se = np.nan
        if np.isfinite(val):
            support_w = float(node.get("supportWeight", 0.0))
            portal_w = float(node.get("portalSubtreeWeight", np.nan))

            if not np.isfinite(portal_w):
                p_cond = float(node.get("portalSubtreeProbabilityConditional", 0.0))
                portal_w = support_w * p_cond

            N_i = N_total * support_w / total_w
            N_cond = N_total * portal_w / total_w

            if key == "supportProbability":
                p = val
                if N_total > 1:
                    se = math.sqrt(max(0.0, p * (1 - p)) / N_total)

            elif key == "portalSubtreeProbabilityConditional":
                p = val
                if N_i > 1:
                    se = math.sqrt(max(0.0, p * (1 - p)) / N_i)

            elif key == "meanSubtreeRoomCountConditional":
                if N_i > 1 and support_w > 0:
                    sum_sq = float(node.get("sumSubtreeRoomCountSq", 0.0))
                    var_pop = (sum_sq / support_w) - val * val
                    var_pop = max(0.0, var_pop)
                    se = math.sqrt(var_pop / N_i)

            elif key == "meanPortalDepthAbsConditional":
                if N_cond > 1 and portal_w > 0:
                    sum_sq = float(node.get("sumPortalDepthAbsSq", 0.0))
                    var_pop = (sum_sq / portal_w) - val * val
                    var_pop = max(0.0, var_pop)
                    se = math.sqrt(var_pop / N_cond)

            elif key == "meanPortalDepthRelConditional":
                if N_cond > 1 and portal_w > 0:
                    sum_sq = float(node.get("sumPortalDepthRelSq", 0.0))
                    var_pop = (sum_sq / portal_w) - val * val
                    var_pop = max(0.0, var_pop)
                    se = math.sqrt(var_pop / N_cond)

            elif key == "meanExpansionOrderConditional":
                if N_i > 1 and support_w > 0:
                    sum_sq = float(node.get("sumExpansionOrderSq", 0.0))
                    var_pop = (sum_sq / support_w) - val * val
                    var_pop = max(0.0, var_pop)
                    se = math.sqrt(var_pop / N_i)

        ses.append(se)

    return np.array(vals, dtype=float), np.array(ses, dtype=float)

def get_debug_values(report, key):
    debug_root = report.get("debugPerNode", {})
    arr = debug_root.get(key, None)
    if arr is None:
        return None
    vals = []
    for v in arr:
        vals.append(np.nan if v is None else float(v))
    return np.array(vals, dtype=float)

def format_prob_with_se(p, se):
    if not np.isfinite(p):
        return "NA"
    if not np.isfinite(se):
        se = 0.0
    if p == 0:
        return "0%"
    if p >= 0.01 or se >= 0.01:
        return f"{p*100:.1f}%\n±{se*100:.1f}%"
    if p >= 0.0001 or se >= 0.0001:
        return f"{p*100:.2f}%\n±{se*100:.2f}%"
    return f"{p*100:.4f}%\n±{se*100:.4f}%"

def format_debug_line(name, value, fmt=".3f"):
    if not np.isfinite(value):
        return f"{name}=NA"
    return f"{name}={format(float(value), fmt)}"

def format_prob_cell(p):
    if not np.isfinite(p):
        return "NA"
    if p == 0:
        return "0%"
    if p >= 0.01:
        return f"{p * 100:.1f}%"
    if p >= 0.0001:
        return f"{p * 100:.2f}%"
    return f"{p * 100:.4f}%"

def make_count_extra_label(appearance=None, wins=None):
    def _fn(i, node):
        parts = []
        if appearance is not None and np.isfinite(appearance[i]):
            parts.append(f"a={int(round(float(appearance[i])))}")
        if wins is not None and np.isfinite(wins[i]):
            parts.append(f"w={int(round(float(wins[i])))}")
        return "\n".join(parts) if parts else None
    return _fn

def shorten_type_name(typ):
    if typ == "FIVE_WAY_CROSSING":
        return "FIVE_WAY"
    return (
        typ.replace("_CORRIDOR", "_CORR")
           .replace("_STAIRS", "_STRS")
           .replace("BRANCHABLE", "BRANCH")
    )


def make_value_label(is_prob=False, fmt=".2f"):
    def _label(i, node, val, se):
        typ = shorten_type_name(node["determinedType"])

        if not np.isfinite(val):
            return f"{i}\n{typ}\nNA"

        if is_prob:
            v_str = format_prob_cell(val)
            s_str = format_prob_cell(se) if np.isfinite(se) and se > 0 else ""
        else:
            v_str = format(float(val), fmt)
            s_str = format(float(se), fmt) if np.isfinite(se) and se > 0 else ""

        if s_str:
            return f"{i}\n{typ}\n{v_str}±{s_str}"
        return f"{i}\n{typ}\n{v_str}"

    return _label


def truncate_cmap(cmap_name, minval=0.0, maxval=1.0, n=256):
    base = plt.get_cmap(cmap_name)
    new_colors = base(np.linspace(minval, maxval, n))
    return colors.LinearSegmentedColormap.from_list(
        f"{cmap_name}_trunc_{minval:.2f}_{maxval:.2f}",
        new_colors
    )


def pick_text_color(rgba):
    r, g, b = rgba[:3]
    luminance = 0.299 * r + 0.587 * g + 0.114 * b
    return "black" if luminance > 0.55 else "white"


def make_plain_log_formatter():
    def _fmt(x, pos=None):
        if x <= 0:
            return ""
        if abs(x - round(x)) < 1e-12:
            return f"{int(round(x))}"
        return f"{x:g}"
    return mticker.FuncFormatter(_fmt)


class TreePlotter:
    def __init__(self, nodes):
        self.nodes = nodes
        self.children, self.parent, self.roots, self.edge_slot = build_tree(nodes)
        self.pos, self.max_depth, self.max_width, self.depth = compute_layout(self.children, self.roots)

    def _make_figure(self, aux_width = 8):
        fig_w = max(16, self.max_width * 1.45 + 4.0)
        fig_h = max(9, (self.max_depth + 1) * 1.95)

        fig = plt.figure(figsize=(fig_w, fig_h), dpi=150)
        gs = fig.add_gridspec(1, 2, width_ratios=[32, aux_width], wspace=0.03)

        ax = fig.add_subplot(gs[0, 0])
        aux_ax = fig.add_subplot(gs[0, 1])

        aux_ax.set_xticks([])
        aux_ax.set_yticks([])
        aux_ax.set_frame_on(False)

        fig.subplots_adjust(left=0.03, right=0.98, top=0.94, bottom=0.04)
        return fig, ax, aux_ax

    def _draw_edges(self, ax, show_edge_slot=False):
        for u, chs in self.children.items():
            x1, y1 = self.pos[u]
            for v in chs:
                x2, y2 = self.pos[v]
                ax.plot([x1, x2], [y1, y2], color="gray", linewidth=1.2, zorder=1)

                if show_edge_slot:
                    mx, my = (x1 + x2) / 2, (y1 + y2) / 2
                    ax.text(
                        mx, my + 0.08, str(self.edge_slot[(u, v)]),
                        fontsize=8, color="dimgray",
                        ha="center", va="center"
                    )

    def draw_metric(
        self,
        values,
        ses,
        title,
        save_path=None,
        cmap="viridis",
        cmap_range=(0.0, 1.0),
        norm_type="linear",
        gamma=0.5,
        is_prob=False,
        value_fmt=".2f",
        show_edge_slot=False,
        colorbar_label=None,
        vmin=None,
        vmax=None,
        annotate=True,
        nan_color="#E6E6E6",
        plain_log_colorbar_labels=False,
        extra_label_fn=None,
        show=True,
    ):
        values = np.array(values, dtype=float)
        finite = np.isfinite(values)

        if finite.any():
            auto_vmin = float(np.nanmin(values))
            auto_vmax = float(np.nanmax(values))
            if vmin is None:
                vmin = auto_vmin
            if vmax is None:
                vmax = auto_vmax
            if abs(vmax - vmin) < 1e-12:
                vmax = vmin + 1.0
        else:
            if vmin is None:
                vmin = 0.0
            if vmax is None:
                vmax = 1.0

        if norm_type == "log":
            vmin = max(vmin, 1e-3)
            norm = colors.LogNorm(vmin=vmin, vmax=vmax)
        elif norm_type == "power":
            norm = colors.PowerNorm(gamma=gamma, vmin=vmin, vmax=vmax)
        else:
            norm = colors.Normalize(vmin=vmin, vmax=vmax)

        cmap_obj = truncate_cmap(cmap, cmap_range[0], cmap_range[1])

        fig, ax, aux_ax = self._make_figure(aux_width=1.2)
        self._draw_edges(ax, show_edge_slot=show_edge_slot)

        n = len(self.nodes)
        node_size = 3400 if n <= 80 else 2200
        font_size = 7.2 if n <= 80 else 5.8

        label_fn = make_value_label(is_prob=is_prob, fmt=value_fmt)

        for i, node in enumerate(self.nodes):
            x, y = self.pos[i]
            if finite[i]:
                color = cmap_obj(norm(values[i]))
            else:
                if nan_color == "cmap_min":
                    color = cmap_obj(0.0)
                elif nan_color == "cmap_max":
                    color = cmap_obj(1.0)
                else:
                    color = nan_color

            ax.scatter(
                [x], [y],
                s=node_size,
                color=color,
                edgecolors="black",
                linewidths=1.0,
                zorder=2
            )

            if annotate:
                label = label_fn(i, node, values[i], ses[i])
                if extra_label_fn is not None:
                    extra = extra_label_fn(i, node)
                    if extra:
                        label = label + "\n" + extra
                ax.text(
                    x, y, label,
                    ha="center", va="center",
                    fontsize=font_size,
                    zorder=3,
                    color=pick_text_color(color) if (finite[i] or nan_color in ("cmap_min", "cmap_max")) else "black"
                )

        sm = plt.cm.ScalarMappable(norm=norm, cmap=cmap_obj)
        sm.set_array([])
        cbar = fig.colorbar(sm, cax=aux_ax)
        if colorbar_label is not None:
            cbar.set_label(colorbar_label)

        if norm_type == "log" and plain_log_colorbar_labels:
            cbar.locator = mticker.LogLocator(base=10)
            cbar.formatter = make_plain_log_formatter()
            cbar.update_ticks()

        ax.set_title(title)
        ax.axis("off")

        if save_path is not None:
            save_path = Path(save_path)
            save_path.parent.mkdir(parents=True, exist_ok=True)
            fig.savefig(save_path)
            print(f"saved: {save_path}")

        if show:
            plt.show()
        else:
            plt.close(fig)

    def draw_categories(
        self,
        categories,
        title,
        save_path=None,
        palette="tab20",
        show_edge_slot=False,
        annotate=True,
        legend_fontsize=8,
        legend_markersize=8,
        extra_label_fn=None,
        show=True,
    ):
        categories = list(categories)
        uniq = list(dict.fromkeys(categories))
        cmap = plt.get_cmap(palette)
        color_map = {c: cmap(i % 20) for i, c in enumerate(uniq)}

        fig, ax, aux_ax = self._make_figure(aux_width=8)
        self._draw_edges(ax, show_edge_slot=show_edge_slot)

        n = len(self.nodes)
        node_size = 3400 if n <= 80 else 2200
        font_size = 7.4 if n <= 80 else 6.0

        for i, node in enumerate(self.nodes):
            x, y = self.pos[i]
            cat = categories[i]
            color = color_map[cat]

            ax.scatter(
                [x], [y],
                s=node_size,
                color=color,
                edgecolors="black",
                linewidths=1.0,
                zorder=2
            )

            if annotate:
                typ = shorten_type_name(cat)
                lines = [str(i), typ]
                if extra_label_fn is not None:
                    extra = extra_label_fn(i, node)
                    if extra:
                        lines.append(extra)
                ax.text(
                    x, y,
                    "\n".join(lines),
                    ha="center", va="center",
                    fontsize=font_size,
                    zorder=3
                )

        handles = [
            Line2D(
                [0], [0],
                marker='o',
                color='w',
                markerfacecolor=color_map[c],
                markeredgecolor='black',
                markersize=legend_markersize,
                label=c
            )
            for c in uniq
        ]

        aux_ax.legend(
            handles=handles,
            loc="upper left",
            fontsize=legend_fontsize,
            frameon=True,
            borderpad=1.2,
            labelspacing=1.0,
            handlelength=1.8,
            handletextpad=0.8
        )

        ax.set_title(title)
        ax.axis("off")

        if save_path is not None:
            save_path = Path(save_path)
            save_path.parent.mkdir(parents=True, exist_ok=True)
            fig.savefig(save_path)
            print(f"saved: {save_path}")

        if show:
            plt.show()
        else:
            plt.close(fig)


def plot_piece_type_probability_heatmap(report, save_path=None, show=True):
    """
    draws posterior piece-type absolute probability heatmap:
    - only draws determinedType == UNKNOWN nodes
    - for concrete types:
        P(type=t) = P(node exists) * P(type=t | node exists)
    - for NO_REAL_PIECE:
        P(no real piece at this hash) = 1 - P(node exists)
      note that this is not equivalent to NONE。
      under the current observation context, it may be
        - NONE
        - or the branch was not decided to generate
    - stderr uses uniqueAncestors as sample size for binomial approximation:
        SE = sqrt(p(1-p)/N_eff)
    """
    nodes = report["nodes"]
    selected = [i for i, node in enumerate(nodes) if node["determinedType"] == "UNKNOWN"]
    if not selected:
        print("No UNKNOWN nodes, skip posterior piece-type heatmap.")
        return
    selected = sorted(selected, key=lambda i: (nodes[i]["treeDepth"], i))
    n_rows = len(selected)
    n_cols = len(HEATMAP_TYPE_ORDER)
    mat = np.zeros((n_rows, n_cols), dtype=float)
    se_mat = np.zeros((n_rows, n_cols), dtype=float)
    ylabels = []
    N_eff = float(report.get("uniqueAncestors", report.get("rawSampleCount", 1)))
    if N_eff <= 0:
        N_eff = 1.0
    for row, idx in enumerate(selected):
        node = nodes[idx]
        support = float(node.get("supportProbability", 0.0))
        probs_cond = node.get("pieceTypeProbability", {})
        for col, typ in enumerate(HEATMAP_TYPE_ORDER):
            if typ == "NO_REAL_PIECE":
                p = max(0.0, min(1.0, 1.0 - support))
            else:
                p = support * float(probs_cond.get(typ, 0.0))
                p = max(0.0, min(1.0, p))
            mat[row, col] = p
            se_mat[row, col] = math.sqrt(max(0.0, p * (1.0 - p)) / N_eff)
        ylabels.append(f"{idx} (d={node['treeDepth']})")
    # adjusts width based on column count and lable size
    fig_w = max(11.0, n_cols * 1.15)
    fig_h = max(5.5, n_rows * 0.62 + 1.4)
    fig, ax = plt.subplots(figsize=(fig_w, fig_h), dpi=160)
    cmap_obj = truncate_cmap("magma", 0.05, 0.95)
    norm = colors.PowerNorm(gamma=0.4, vmin=0.0, vmax=1.0)
    im = ax.imshow(mat, aspect="auto", cmap=cmap_obj, norm=norm)
    ax.set_xticks(range(n_cols))
    ax.set_xticklabels(HEATMAP_TYPE_ORDER, rotation=45, ha="right")
    ax.set_yticks(range(n_rows))
    ax.set_yticklabels(ylabels, fontsize=8)
    # write absolute probability ± stderr
    for r in range(n_rows):
        for c in range(n_cols):
            p = mat[r, c]
            se = se_mat[r, c]
            bg = cmap_obj(norm(p))
            ax.text(
                c, r,
                format_prob_with_se(p, se),
                ha="center", va="center",
                fontsize=5.7,
                color=pick_text_color(bg)
            )
    ax.set_title("Posterior piece-type absolute probability (only prior-UNKNOWN nodes)")
    plt.colorbar(im, ax=ax, fraction=0.025, pad=0.02, label="Absolute probability")
    # avoid lables on the bottom being cut off
    fig.subplots_adjust(
        left=0.12,
        right=0.98,
        top=0.92,
        bottom=max(0.22, min(0.38, 0.12 + 0.012 * max(len(x) for x in HEATMAP_TYPE_ORDER)))
    )
    if save_path is not None:
        save_path = Path(save_path)
        save_path.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(save_path, bbox_inches="tight", pad_inches=0.15)
        print(f"saved: {save_path}")
    if show:
        plt.show()
    else:
        plt.close(fig)


def plot_default_report(report, output_dir="plots", show=False):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    obs_nodes = get_observation_nodes(report)
    plotter = TreePlotter(obs_nodes)
    debug_log_importance = get_debug_values(report, "nodeLogMeanImportanceWeightDelta")
    debug_delta_var = get_debug_values(report, "nodeImportanceWeightDeltaVariance")
    debug_gamma = get_debug_values(report, "nodeGamma")
    debug_log_gamma = get_debug_values(report, "nodeLogGamma")
    debug_win_count = get_debug_values(report, "nodeWinCount")
    debug_appearance_count = get_debug_values(report, "nodeAppearanceCount")

    debug_empirical_win_rate = None
    if debug_win_count is not None and debug_appearance_count is not None:
        debug_empirical_win_rate = np.full_like(debug_win_count, np.nan)
        mask = np.isfinite(debug_appearance_count) & (debug_appearance_count > 0)
        debug_empirical_win_rate[mask] = debug_win_count[mask] / debug_appearance_count[mask]
    
    debug_log_inv_ress = None
    if debug_log_importance is not None and debug_delta_var is not None:
        debug_log_inv_ress = np.full_like(debug_delta_var, np.nan)
        valid = np.isfinite(debug_log_importance) & np.isfinite(debug_delta_var) & (debug_delta_var >= 0.0)
        positive_var = valid & (debug_delta_var > 0.0)
        # when variance = 0，CV^2 = 0，log(1/rESS) = log(1 + 0) = 0
        debug_log_inv_ress[valid] = 0.0
        # log(CV^2) = log(var) - 2 * log(mean)
        debug_log_cv2 = np.full_like(debug_delta_var, -np.inf)
        debug_log_cv2[positive_var] = np.log(debug_delta_var[positive_var]) - 2.0 * debug_log_importance[positive_var]
        # log(1/rESS) = log(1 + CV^2) = logaddexp(0, log(CV^2))
        debug_log_inv_ress[positive_var] = np.logaddexp(0.0, debug_log_cv2[positive_var])

    # 1) observation type
    plotter.draw_categories(
        [node["determinedType"] for node in obs_nodes],
        title="Observation determined types",
        save_path=output_dir / "observation_types.png",
        show_edge_slot=True,
        legend_fontsize=16,
        legend_markersize=16,
        show=show
    )

    if debug_log_importance is not None:
        plotter.draw_metric(
            debug_log_importance,
            np.full_like(debug_log_importance, np.nan),
            title="Debug: logImportanceWeight",
            save_path=output_dir / "debug_nodeLogMeanImportanceWeightDelta.png",
            cmap="magma_r",
            cmap_range=(0.05, 1.0),
            norm_type="linear",
            is_prob=False,
            value_fmt=".3f",
            colorbar_label="logImportanceWeight",
            nan_color="#E6E6E6",
            show_edge_slot=True,
            show=show
        )

    if debug_log_inv_ress is not None:
        plotter.draw_metric(
            debug_log_inv_ress,
            np.full_like(debug_log_inv_ress, np.nan),
            title="Debug: log(1/rESS) from ImportanceWeightDelta",
            save_path=output_dir / "debug_nodeLogInvRelativeESSFromImportanceWeightDelta.png",
            cmap="inferno",
            cmap_range=(0.05, 1.0),
            norm_type="linear",
            is_prob=False,
            value_fmt=".3f",
            colorbar_label="log(1/rESS)",
            nan_color="#E6E6E6",
            vmin=0.0,
            show_edge_slot=True,
            show=show
        )

    if debug_delta_var is not None:
        plotter.draw_metric(
            debug_delta_var,
            np.full_like(debug_delta_var, np.nan),
            title="Debug: Var(importanceWeightDelta)",
            save_path=output_dir / "debug_nodeImportanceWeightDeltaVariance.png",
            cmap="magma",
            cmap_range=(0.05, 1.0),
            norm_type="power",
            is_prob=False,
            value_fmt=".4f",
            colorbar_label="Variance",
            nan_color="#E6E6E6",
            vmin=0.0,
            show_edge_slot=True,
            show=show
        )

    if debug_log_gamma is not None and np.isfinite(debug_log_gamma).any():
        vmax_abs = float(np.nanmax(np.abs(debug_log_gamma[np.isfinite(debug_log_gamma)])))
        vmax_abs = max(vmax_abs, 1e-6)

        plotter.draw_metric(
            debug_log_gamma,
            np.full_like(debug_log_gamma, np.nan),
            title="Debug: learned log(gamma)",
            save_path=output_dir / "debug_nodeLogGamma.png",
            cmap="coolwarm",
            cmap_range=(0.0, 1.0),
            norm_type="linear",
            is_prob=False,
            value_fmt=".3f",
            colorbar_label="log(gamma)",
            nan_color="#E6E6E6",
            vmin=-vmax_abs,
            vmax=vmax_abs,
            show_edge_slot=True,
            extra_label_fn=make_count_extra_label(debug_appearance_count, debug_win_count),
            show=show
        )

    if debug_gamma is not None:
        plotter.draw_metric(
            debug_gamma,
            np.full_like(debug_gamma, np.nan),
            title="Debug: learned gamma",
            save_path=output_dir / "debug_nodeGamma.png",
            cmap="coolwarm",
            cmap_range=(0.0, 1.0),
            norm_type="log",
            is_prob=False,
            value_fmt=".3f",
            colorbar_label="gamma",
            nan_color="#E6E6E6",
            vmin=1e-1,
            vmax=1e+1,
            show_edge_slot=True,
            extra_label_fn=make_count_extra_label(debug_appearance_count, debug_win_count),
            plain_log_colorbar_labels=True,
            show=show
        )

    if debug_empirical_win_rate is not None:
        plotter.draw_metric(
            debug_empirical_win_rate,
            np.full_like(debug_empirical_win_rate, np.nan),
            title="Debug: empirical win rate among pending-set appearances",
            save_path=output_dir / "debug_nodeEmpiricalWinRate.png",
            cmap="inferno",
            cmap_range=(0.05, 1.0),
            norm_type="power",
            gamma=0.5,
            is_prob=True,
            value_fmt=".3f",
            colorbar_label="Win Rate",
            nan_color="#E6E6E6",
            vmin=0.0,
            vmax=1.0,
            show_edge_slot=True,
            extra_label_fn=make_count_extra_label(debug_appearance_count, debug_win_count),
            show=show
        )

    # 2) tree graphs
    metric_specs = [
        {
            "key": "supportProbability",
            "title": "Node existence probability",
            "cmap": "magma",
            "cmap_range": (0.1, 1.0),
            "norm_type": "linear",
            "is_prob": True,
            "cbar": "Probability",
            "nan_color": "#E6E6E6",
            "vmin": 0.0,
            "vmax": None
        },
        {
            "key": "portalSubtreeProbabilityConditional",
            "title": "P(portal in subtree | node exists)",
            "cmap": "inferno",
            "cmap_range": (0.1, 1.0),
            "norm_type": "power",
            "gamma": 0.4,
            "is_prob": True,
            "cbar": "Probability",
            "nan_color": "#E6E6E6"
        },
        {
            "key": "meanSubtreeRoomCountConditional",
            "title": "Mean subtree room count | node exists",
            "cmap": "viridis",
            "cmap_range": (0.0, 1.0),
            "norm_type": "log",
            "is_prob": False,
            "fmt": ".1f",
            "cbar": "Rooms",
            "nan_color": "#E6E6E6",
            "plain_log_colorbar_labels": True
        },
        {
            "key": "meanExpansionOrderConditional",
            "title": "Mean expansion order | node exists",
            "cmap": "viridis_r",
            "cmap_range": (0.05, 0.95),
            "norm_type": "linear",
            "is_prob": False,
            "fmt": ".1f",
            "cbar": "Expansion Order",
            "nan_color": "#E6E6E6",
            "vmin": 0.0
        },
        {
            "key": "meanPortalDepthRelConditional",
            "title": "Mean distance to portal | portal in subtree",
            "cmap": "viridis_r",
            "cmap_range": (0.0, 0.9),
            "norm_type": "linear",
            "is_prob": False,
            "fmt": ".2f",
            "cbar": "Depth",
            "nan_color": "cmap_max"
        },
        {
            "key": "meanPortalDepthAbsConditional",
            "title": "Mean absolute portal depth | portal in subtree",
            "cmap": "magma_r",
            "cmap_range": (0.0, 0.9),
            "norm_type": "linear",
            "is_prob": False,
            "fmt": ".2f",
            "cbar": "Depth",
            "nan_color": "cmap_max"
        },
    ]

    for spec in metric_specs:
        vals, ses = get_metric_and_se(report, spec["key"])
        plotter.draw_metric(
            vals,
            ses,
            title=f"{spec['title']} (N_eff={report.get('uniqueAncestors', 'Unknown')})",
            save_path=output_dir / f"{spec['key']}.png",
            cmap=spec["cmap"],
            cmap_range=spec["cmap_range"],
            norm_type=spec.get("norm_type", "linear"),
            gamma=spec.get("gamma", 0.5),
            is_prob=spec.get("is_prob", False),
            value_fmt=spec.get("fmt", ".2f"),
            colorbar_label=spec["cbar"],
            nan_color=spec.get("nan_color", "#E6E6E6"),
            vmin=spec.get("vmin", None),
            vmax=spec.get("vmax", None),
            plain_log_colorbar_labels=spec.get("plain_log_colorbar_labels", False),
            show_edge_slot=True,
            show=show
        )

    # 3) posterior piece-type heat map
    plot_piece_type_probability_heatmap(
        report,
        save_path=output_dir / "piece_type_probability_heatmap.png",
        show=show
    )

    print("Overall summary:")
    print(json.dumps(report["overall"], indent=2, ensure_ascii=False))


if __name__ == "__main__":
    report = load_stats_report("outputs/stronghold_stats_merged.json")
    plot_default_report(report, output_dir="plots", show=False)