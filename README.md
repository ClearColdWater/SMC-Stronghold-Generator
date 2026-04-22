# SMC-Stronghold-Generator

Generates imaginary Minecraft strongholds consistent with a given observation, without introducing sampling bias.

This tool does **not** recover the unique real seed behind an observed stronghold.

Instead, it samples **imaginary strongholds** from the corresponding conditional random process under the supplied observation constraints.

This project is intended to help players evaluate which unexplored branches are more likely to be valuable, and by how much.

It is not intended to recover the one and only possible seed from an observation.

## Releases and Platform Support

|                                                                 Platform                                                                    |    Release Contents    |                   Requirements                   |
|---------------------------------------------------------------------------------------------------------------------------------------------|------------------------|--------------------------------------------------|
| [Windows x64](https://github.com/ClearColdWater/SMC-Stronghold-Generator/releases/download/v0.1.0-beta.1/stronghold-cli-windows-x64.zip)    | CLI + Plotting Bundled |                       None                       |
| [Linux x64](https://github.com/ClearColdWater/SMC-Stronghold-Generator/releases/download/v0.1.0-beta.1/stronghold-cli-linux-x64.tar.gz)     |      CLI Bundled       | Requires local Python 3 + `numpy` + `matplotlib` |
| [macOS arm64](https://github.com/ClearColdWater/SMC-Stronghold-Generator/releases/download/v0.1.0-beta.1/stronghold-cli-macos-arm64.tar.gz) |      CLI Bundled       | Requires local Python 3 + `numpy` + `matplotlib` |

## Quick Start

The demo mode automatically generates a valid observation and sample for it.

### Windows

1. Download and extract the Windows release ZIP.
2. Open the extracted folder.
3. Click the folder path bar in File Explorer.
4. Type `cmd` and press Enter.
5. Run:

```bash
stronghold-cli --demo --plot
```

### Linux / macOS

1. Download and extract the release archive, then open a terminal in that folder.
2. Install the required plotting dependencies:

```bash
python3 -m pip install -r requirements.txt
```

*(If python3 is not installed, install it via your distribution's package manager, Homebrew, or python.org).*

3. Run:

```bash
./stronghold-cli --demo --plot
```

---

## Common commands

(Note: The examples below use the Windows `stronghold-cli` command. On Linux/macOS, use `./stronghold-cli` instead.)

### Run with your own observation file and plot

```bash
stronghold-cli "path/to/observation.json" --plot
```

### Show help

```bash
stronghold-cli -h
```

---

## Observation JSON Format

You can call generateObservation() to get a random stronghold and generate an observation from it as a demo, or put your json observation file under `/observations`.

An observation file is a JSON object with this overall structure:

```json
{
    "starterDirection": "SOUTH",
    "tree": {
        "totalNodes": 4,
        "nodes": [
            {
                "ch": [1],
                "customData": 0,
                "determinedType": "STARTER_STAIRS"
            },
            {
                "branchGenerateWeights": [1.0, 0.0, 0.0, 0.0, "inf"],
                "ch": [2, -1, -1, -1, 3],
                "customData": 0,
                "determinedType": "FIVE_WAY_CROSSING"
            },
            {
                "ch": [-2],
                "customData": 0,
                "determinedType": "PRISON_CELL"
            },
            {
                "branchGenerateWeights": ["inf", "inf", "inf"],
                "ch": [-2, -2, -2],
                "customData": 0,
                "determinedType": "BRANCHABLE_CORRIDOR"
            }
        ]
    }
}
```

## Top-Level Fields

### `starterDirection`

Direction of the starter piece.
Allowed values:

- `"NORTH"`
- `"EAST"`
- `"SOUTH"`
- `"WEST"`

Use only these exact strings.

---

### `tree.totalNodes`

The total number of nodes stored in the observation.

This must match the actual number of entries in `tree.nodes`.

---

### `tree.nodes`

Array of node objects.

- Node indices are zero-based.
- Child references inside `ch` point to indices in this array.
- `nodes[0]` is expected to be the root (starter) node.

### Recommendation: include at least the first two nodes

It is **strongly recommended** that every observation explicitly includes at least the first two nodes of the stronghold tree.
In other words:

- include the root starter node
- include its first successor (the five-way) as well

Observations that omit one of these first two nodes have not been tested.

If you want to generate vanilla strongholds, use `Stronghold<vanilla>` instead.

---

## Node Fields

Each node object contains the following fields.

### `ch`

Child reference array.

Example:

```json
"ch": [5, -2, -1]
```

The meaning of each entry is:

- `>= 0` — **if** this branch generates, it leads to that child node index in `tree.nodes`
- `-1` — branch is known **not** to generate
- `-2` — branch state is **unresolved**, and the child (if any) is **unobserved further**

A non-negative child index does not by itself mean that the branch is forced to generate;
it means that the observation for that child is available if that branch exists.

This `-2` value is important:

- use `-1` when you know the branch does not exist
- use `-2` when you do **not know** whether the branch exists, or when the branch exists conceptually but was not followed / observed further

So:

- `-1` = confirmed absence
- `-2` = uncertainty / observation stops here

Note that for both `ch >= 0` and `ch == -2`, you can specify the likelihood of that branch generating with the field `"branchGenerateWeights"` which will be addressed later.

#### Short form is allowed

`ch` does **not** need to contain all 5 entries.
If fewer than 5 entries are provided, the remaining entries are automatically filled as `-1`.
For example:

```json
"ch": [123]
```

is valid and is interpreted like:

```json
"ch": [123, -1, -1, -1, -1]
```

This is convenient for piece types that uses fewer than 5 branches.

#### Important note about `"determinedType": "UNKNOWN"`

During import, the observation is post-processed by the program's internal build step.

In particular, nodes whose `determinedType` is `"UNKNOWN"` will have their children refilled with `-2`.

In practice, `determinedType: "UNKNOWN"` nodes should be the leaves of the observation tree.

This is because you should not be able to have meaningful observation to a node's child when you don't even know the type of that node.

So if a branch is genuinely unobserved, prefer `-2` over inventing fake child nodes.

---

### How to decide the branch index of each child

#### No outgoing branch

These do not have child nodes:

- `SMALL_CORRIDOR`
- `LIBRARY`
- `PORTAL_ROOM`
- `NONE`
For these, write:

```json
"ch": []
```

#### Typically one outgoing branch

These only use `ch[0]`:

- `CHEST_CORRIDOR`
- `LEFT_TURN`
- `RIGHT_TURN`
- `PRISON_CELL`
- `SPIRAL_STAIRS`
- `STRAIGHT_STAIRS`
- `STARTER_STAIRS`

For these, write:

```json
"ch": [123]
```

#### Up to three branches

These generally use the first three slots(`ch[0], ch[1], ch[2]`):

- `ROOM_CROSSING`
- `BRANCHABLE_CORRIDOR`

The forward branch is always branch `0`.
The branch indexes for the left and right branches are directional.

The branch facing the negative cardinal direction is always branch `1`;
the branch facing the positive cardinal direction is always branch `2`,
regardless of the negative branch generated or not.

For example, you are in a `BRANCHABLE_CORRIDOR` room,
going in from starter facing `SOUTH`(`positive Z`),
the branch in front is naturally branch `0`.

The branch on the left is facing `EAST`(`positive X`), so it is branch `2`;
the branch on the right is facing `WEST`(`negative X`), so it is branch `1`.
Even if the branch on the right was not even decided to generate
(the bricks are not carved), the branch on the left is still branch `2`.

---

#### Up to five branches

`FIVE_WAY_CROSSING` uses all five child slots.

The forward branch is always branch `0`.
The branch indexes for the other four branches are directional.

The two branches in the same group(both on the left or right side) facing the negative cardinal direction are always branch `1` and `2`;
the two branches in the same group(both on the left or right side) facing the positive cardinal direction are always branch `3` and `4`,
regardless of other branches generated or not.

Inside each group,
The branch on the relative negative cardinal position is branch `x`;
the branch on the relative positive cardinal position is branch `x + 1`,
again regardless of other branches generated or not.

For example, you are going in a `FIVE_WAY_CROSSING` from starter facing `SOUTH`(`positive Z`),
the branch in front is naturally branch `0`.

The branch on the left is facing `EAST`(`positive X`), so the two branches on the left are branch `3` and `4`;
the branch on the right is facing `WEST`(`negative X`), so the two branches on the right are branch `1` and `2`.
Even if none the branches on the right were not even decided to generate
(the bricks are not carved), the branch on the left are still branch `3` and `4`.

Then to decide which branch on the right is branch `1` and which is `2`,
the bottom right branch is to the `NORTH`(`negative Z`) of the top right branch,
so it will be branch `1` and the top right branch will be branch `2`.
Similarly the bottom left branch is branch `3` and top left is branch `4`.

---

### `branchGenerateWeights`

Relative weights for **branch generation versus branch non-generation**.

For each branch slot:

- generation weight = given value
- non-generation weight = `1.0`

This means the field is interpreted as a ratio-like weight, not as a probability.

#### Interpretation

- `1.0` — neutral, probability of branch generating stays vanilla
- `> 1.0` — favors generation
- `< 1.0` — disfavors generation
- `0.0` — branch will never generate
- `"inf"` — branch is forced to generate

Note that the value is only relevant for branch slots that have a given probability of generating.

Such branches in the stronghold include all side branches of a five-way and all side branches of a branchable corridor.

#### Special `"inf"` rule

JSON does not support numeric infinity directly, so the string:

```json
"inf"
```

is used to represent positive infinity.

Example:

```json
"branchGenerateWeights": [1.0, "inf", 0.5]
```

This means:

- branch 0: neutral
- branch 1: forced generation
- branch 2: generation disfavored

#### Short form is allowed

If fewer than 5 values are provided, the missing trailing entries are automatically filled with `1.0`.

So this:

```json
"branchGenerateWeights": [2.0]
```

is interpreted as:

```json
"branchGenerateWeights": [2.0, 1.0, 1.0, 1.0, 1.0]
```

### Recommendation convention

For branch slots whose generation is already fixed by the room type, `branchGenerateWeights` usually does not matter.

For readability, you may still choose to write:

- `"inf"` for branches that are forced to generate
- `0.0` for branches that are impossible to generate

but this is a documentation convention rather than a requirement.

#### Optional field

This field may be omitted entirely.
If omitted, all five branch generation weights default to `1.0`.

---

### `customData`

Piece-specific auxiliary integer data.
Example:

```json
"customData": 0
```

This field is optional. If omitted, it defaults to `0`.

It is currently used in the following cases:

- `SmallCorridor.length`
- `Library.isSmall`

For other pieces, it is ignored.

---

### `determinedType`

The observed or assigned type of this node.
Allowed values:

- `"CHEST_CORRIDOR"`
- `"SMALL_CORRIDOR"`
- `"FIVE_WAY_CROSSING"`
- `"LEFT_TURN"`
- `"RIGHT_TURN"`
- `"ROOM_CROSSING"`
- `"LIBRARY"`
- `"PORTAL_ROOM"`
- `"PRISON_CELL"`
- `"SPIRAL_STAIRS"`
- `"STRAIGHT_STAIRS"`
- `"BRANCHABLE_CORRIDOR"`
- `"STARTER_STAIRS"`
- `"NONE"`
- `"UNKNOWN"`

General meaning:

- concrete type such as `"LEFT_TURN"` = the node is known to be that type
- `"NONE"` = explicitly no room
- `"UNKNOWN"` = the node's exact type is not determined

### `weights`

Relative likelihood weights over room types.
Example:

```json
"weights": {
  "LEFT_TURN": 1.0,
  "RIGHT_TURN": 1.0,
  "PORTAL_ROOM": 3.0
}
```

Interpretation:

- keys are piece type names
- values are relative likelihood weights(when all pieces have the same weight, their posterior type distribution stays vanilla)
- omitted types are treated as weight `0`

#### If `weights` is omitted

The loader applies these defaults:

- if `determinedType != "UNKNOWN"`:
  - the node becomes a one-hot observation of `determinedType`
- if `determinedType == "UNKNOWN"`:
  - all supported room types receive weight `1.0`

So omitting `weights` is valid.

#### Recommended convention

For readability, keep `weights` consistent with `determinedType`:

- if only one type is possible, use that type as `determinedType`
- if multiple types are possible, use `"UNKNOWN"` and specify the allowed set in `weights`

---

## Minimal Recommended Example

This is a minimal observation that still follows the recommendation of explicitly including the first two nodes.

```json
{
    "starterDirection": "EAST",
    "tree": {
        "totalNodes": 2,
        "nodes": [
            {
                "ch": [1],
                "branchGenerateWeights": [1.0],
                "determinedType": "STARTER_STAIRS"
            },
            {
                "ch": [-2, -2, -2, -2, -2],
                "determinedType": "FIVE_WAY_CROSSING"
            }
        ]
    }
}
```

## License

This project is licensed under the [MIT license](./LICENSE).
See also [third party notices](./THIRD_PARTY_NOTICES.txt) for bundled dependencies.
