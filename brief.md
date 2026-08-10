# Trashavania  
## Game Design & Engineering Brief

**Status:** Hackathon prototype  
**Working title:** *Trashavania: The Adventures of Jimothy*  
**Target platform:** Nintendo Entertainment System–compatible hardware  
**Genre:** Gothic comedy action-platformer  
**Target playtime:** 5–10 minutes  
**Assumption:** 2–3 day hackathon with a small team

## 1. High Concept

Once every century, beneath the light of the Blood Moon, Castle Refuse rises from the municipal landfill.

Jimothy—a brave, hungry, and questionably hygienic raccoon—must cross its haunted halls, recover the legendary Golden Garbage, and defeat the immortal lord of the castle: Count Dumpula.

*Trashavania* is a short, linear 8-bit action-platformer built as a genuine NES-compatible ROM. It combines deliberate gothic platforming with raccoon movement, improvised trash weapons, and extremely serious presentation of extremely unserious events.

## 2. Design Goals

### Primary goals

- Produce a complete NES ROM that runs in an emulator and on original-compatible hardware.
- Deliver a polished five-minute experience with a beginning, boss fight, and ending.
- Make Jimothy immediately expressive despite strict visual limitations.
- Capture the rhythm of classic gothic action-platformers without copying protected art, music, characters, or level layouts.
- Make trash mechanically useful, not merely decorative.

### Experience pillars

**Gothic grandeur, garbage subject matter**  
The world treats Jimothy’s dumpster raid as an ancient supernatural conflict.

**Simple controls with deliberate movement**  
The player should understand every action immediately, while enemy placement and timing create the challenge.

**Trash is treasure**  
Discarded objects become weapons, health, currency, scenery, and story.

**Short, complete, and replayable**  
Finishing a tiny game is more important than building the first level of a large one.

## 3. Scope

### Must-have

- Title screen
- One continuous stage divided into 4–5 rooms
- Jimothy movement, jumping, crouching, and attacking
- One standard attack
- Two trash sub-weapons
- Three normal enemy types
- Pickups and health
- One complete boss fight
- Death and restart flow
- Victory screen with score or raccoon rank
- Music and sound effects
- Valid NES ROM tested in an accurate emulator

### Stretch goals

- Fourth enemy type
- Miniboss
- Alternate sub-weapon
- Breakable walls containing hidden food
- Speedrun timer
- Hardware demonstration using a flash cartridge
- Multiple end-of-game ranks
- Animated introduction or ending sequence

### Explicit non-goals

- Metroidvania progression
- Large interconnected world
- Save system
- Procedural generation
- Inventory screen
- Multiple playable characters
- Complex cutscenes
- More than one major boss
- Passwords or persistent progression

## 4. Core Game Loop

1. Enter a castle room.
2. Read enemy and hazard placement.
3. Traverse platforms using deliberate jumps.
4. Swipe enemies or spend snacks on a sub-weapon.
5. Scavenge food and useful garbage.
6. Reach the doorway to the next room.
7. Defeat Count Dumpula.
8. Receive a raccoon rank based on time, health, loot, and chaos.

## 5. Controls

| Input | Action |
|---|---|
| D-pad Left/Right | Walk |
| D-pad Down | Crouch / raccoon scrunch |
| A | Jump |
| B | Swipe |
| Up + B | Use equipped trash weapon |
| Start | Pause |
| Select | Unused or cycles sub-weapons as a stretch goal |

Movement should feel deliberate but fair. Jimothy has a firm jump arc, modest air control, short input buffering, and a few frames of edge forgiveness. Authentic presentation is desirable; authentic frustration is not.

## 6. Player Abilities

### Raccoon Swipe

Jimothy’s default close-range attack.

- Fast startup
- Short range
- No resource cost
- Breaks candles, trash bags, and fragile walls
- Can knock small projectiles out of the air

The animation should emphasize tiny arms and unreasonable confidence.

### Jump

A compact, predictable arc designed around tile-based platforms. Jimothy can adjust direction slightly in the air but cannot double-jump.

### Crouch

Allows Jimothy to avoid high projectiles and fit beneath hazards. The compressed “scrunch” silhouette should be one of the game’s signature animations.

### Damage and knockback

Jimothy loses health and receives brief knockback when struck. Knockback should be noticeable without routinely throwing the player into unavoidable death pits.

Recommended health: **8 pips**.

## 7. Trash Weapons

Enemies and containers drop **snacks**, which replace the conventional ammunition resource. Using a trash weapon consumes snacks.

### Bottle Cap

A fast, straight projectile.

- Low cost
- Reliable range
- Useful against flying enemies
- Can activate distant switches if included

### Rotten Tomato

An arcing projectile that splatters on the floor.

- Moderate cost
- Creates a short-lived damage zone
- Useful against groups and stationary targets
- Provides a visually comic substitute for magical fire

### Stretch weapon: Fishbone

Travels forward and returns like a boomerang. More expensive and technically riskier because it requires persistent projectile state.

## 8. Pickups

- **Loose Snack:** Adds one ammunition point
- **Family-Size Snack:** Adds five ammunition points
- **Half-Eaten Burrito:** Restores health
- **Golden Garbage:** High-value score item
- **Mystery Leftovers:** Random health or score reward
- **Trash Weapon Icon:** Changes the equipped sub-weapon

Breakable containers should include candles, garbage bags, cracked bins, and suspicious wall cavities.

## 9. Level Structure

The game uses locked, screen-sized rooms connected by doorway transitions. This keeps camera behavior and collision predictable while reducing rendering complexity.

### Room 1: Garbage Grove

A brief outdoor tutorial.

Introduces:

- Walking and jumping
- Basic attack
- First enemy
- Breakable trash container
- Castle entrance

### Room 2: Recycling Crypt

A horizontal combat room constructed from bins, pipes, and gothic masonry.

Introduces:

- Flying enemies
- Bottle Cap weapon
- Moving or collapsing hazard
- First health pickup

### Room 3: Tower of Cans

A vertical platforming room.

Introduces:

- Falling cans
- Narrow platforms
- Enemies positioned around jump arcs
- Optional hidden Golden Garbage

### Room 4: Chapel of Questionable Leftovers

A short challenge room before the boss.

Introduces:

- Rotten Tomato weapon
- Denser enemy combination
- Health refill near the exit
- Dramatic approach to the final dumpster

### Room 5: The Moonlit Dumpster

Boss arena containing Count Dumpula and the Golden Garbage.

## 10. Enemies

### Trash Bat

A garbage bag twisted into the shape of a bat.

- Moves in a simple wave pattern
- Low health
- Encourages use of ranged attacks
- Drops snacks frequently

### Skeletal Alley Cat

A territorial cat assembled from bones and discarded cutlery.

- Paces on platforms
- Charges when Jimothy approaches
- Cannot turn around during its charge

### Haunted Garden Gnome

A stationary ranged enemy.

- Throws small ceramic projectiles
- Vulnerable between attacks
- Can be avoided or destroyed

### Stretch enemy: Possum Knight

Pretends to die after being hit, then gets back up when the player passes.

## 11. Boss: Count Dumpula

Count Dumpula is an ancient haunted dumpster with glowing eyes, a cape-like garbage bag, and aristocratic contempt for scavengers.

To reduce sprite pressure, the dumpster body can be rendered as background tiles. Only the lid, eyes, hands, projectiles, and damage effects need to be sprites.

### Attack cycle

1. **Lid Slam:** The lid telegraphs and strikes the ground.
2. **Garbage Toss:** Arcing trash projectiles fall into the arena.
3. **Trash Bat Summon:** One or two Trash Bats enter the room.
4. **Open Weak Point:** Count Dumpula’s glowing interior becomes vulnerable.

At half health, the attack cycle accelerates and projectiles use more demanding patterns. The underlying logic should remain predictable.

Recommended boss health: **12–16 hits**.

Defeating Count Dumpula reveals the Golden Garbage. Jimothy raises it triumphantly before immediately attempting to eat it.

## 12. Tone and Writing

The game should present its absurdity with complete sincerity.

Example text:

- “It was a miserable night to have a curse.”
- “Castle Refuse has awakened.”
- “Half a Burrito — Its former purpose is unknowable.”
- “Count Dumpula demands that all refuse remain unclaimed.”
- “Jimothy has no master. Jimothy has snacks.”

Text should remain sparse because of screen space and ROM constraints.

## 13. Art Direction

### Visual style

- Gothic architecture constructed from suburban refuse
- Strong silhouettes
- Limited animation with expressive poses
- Dark blue, purple, gray, and sickly green environments
- Warm food colors used to make pickups readable
- Moonlit exterior scenes and grim interior stonework
- Original assets throughout

### Jimothy

Jimothy must remain recognizable at low resolution through:

- Masked face
- Ringed tail
- Rounded ears
- Low, slightly hunched stance
- Exaggerated attack and crouch poses

Recommended sprite footprint: approximately **16×24 or 16×32 pixels**, assembled from 8×16 hardware sprites.

### Animation budget

Prioritize:

- Idle
- Two-frame walk
- Jump
- Crouch
- Swipe
- Hurt
- Victory pose

Additional frames should only be added after the complete game is playable.

## 14. Audio Direction

Music should combine gothic arpeggios with playful, slightly grimy chiptune instrumentation.

Suggested tracks:

- Title theme
- Main castle theme
- Boss theme
- Victory sting
- Death sting

Reserve audio capacity for clear gameplay cues:

- Swipe
- Jump
- Pickup
- Player damage
- Enemy defeat
- Boss damage
- Door transition
- Dumpster lid slam

Avoid sampled audio for the initial build. The standard pulse, triangle, and noise channels are sufficient.

## 15. Technical Target

### Hardware profile

- NTSC NES-compatible system
- 6502-family CPU
- 256×240 display
- 2 KB internal system RAM
- Tile-based backgrounds
- 64 hardware sprites total
- Maximum of 8 sprites on a scanline before flicker or omission
- 60 Hz fixed-step update loop

### Cartridge configuration

Recommended prototype target:

- **Mapper 2 / UxROM-compatible**
- Banked program ROM
- CHR RAM for loading room-specific graphics
- Horizontal or vertical nametable mirroring selected according to room design

Mapper 2 provides more program space than the simplest cartridge format while remaining relatively straightforward and widely supported.

### Candidate toolchain

- C with small 6502 assembly routines where necessary
- cc65-compatible compiler and linker
- Lightweight NES support library
- Tile and map exporter
- FamiStudio-compatible music workflow
- Accurate emulator for daily testing
- Flash cartridge for final hardware verification

The team should freeze the toolchain before content production begins.

## 16. Runtime Architecture

The game should use a deterministic, fixed-step main loop:

1. Read controller input.
2. Update player state.
3. Update enemies and projectiles.
4. Resolve tile and entity collisions.
5. Update animation.
6. Prepare sprite data.
7. Commit graphics updates during vertical blank.
8. Update audio.

### Game states

- Boot
- Title
- Gameplay
- Room transition
- Pause
- Player death
- Boss defeated
- Ending

### Entity model

Use fixed-size pools rather than dynamic allocation.

Suggested limits:

- 1 player
- 4 active standard enemies
- 3 player or enemy projectiles
- 4 pickups
- 1 boss controller
- 1–2 effects slots

These limits should be treated as part of the level-design language.

### Collision

- Tile-based environment collision
- Simple rectangular entity hitboxes
- Separate attack and damage boxes
- Integer or fixed-point position and velocity
- Collision data stored separately from decorative tiles

### Room data

Each room should define:

- Background tile map
- Collision map
- Palette
- Spawn points
- Door destinations
- Item placements
- Music selection
- Optional room-specific behavior

Room data should be compressed or encoded compactly in ROM and loaded during a masked doorway transition.

## 17. Performance Rules

- Maintain one gameplay update per frame.
- Avoid more than two ordinary enemies sharing Jimothy’s horizontal scanlines.
- Use intentional sprite flicker only when unavoidable.
- Limit simultaneous projectiles.
- Perform expensive room decompression only during transitions.
- Keep background updates small during gameplay.
- Do not introduce scrolling until all fixed-screen rooms work.
- Test frequently in an emulator with sprite-limit and timing diagnostics enabled.

## 18. Development Plan

### Foundation

- Create bootable ROM
- Display background and Jimothy sprite
- Read controller input
- Establish repeatable build process
- Verify emulator compatibility

### Core play

- Implement movement and jumping
- Add tile collision
- Add camera-locked rooms
- Add swipe attack
- Add damage, health, death, and restart

### Content pipeline

- Load room maps and palettes
- Add doors and room transitions
- Add enemy entity system
- Add pickups and snack resource
- Add trash weapons

### Completion

- Implement boss
- Add title, victory, and ending states
- Add music and sound effects
- Tune difficulty and placement
- Remove softlocks and frame overruns
- Test full ROM from power-on to ending

### Hardware validation

- Load the final ROM using compatible hardware
- Confirm input, audio, palette, transitions, and boss behavior
- Complete at least three uninterrupted playthroughs
- Preserve the verified ROM as the submission build

## 19. Definition of Done

The prototype is complete when:

- The ROM boots without manual setup.
- A new player can understand the controls without external instructions.
- The entire stage can be completed.
- Jimothy can attack, take damage, die, and restart.
- At least two trash weapons work.
- Three enemy types behave consistently.
- Count Dumpula has a readable, defeatable attack cycle.
- The game includes music, sound effects, a title screen, and an ending.
- No known softlock prevents completion.
- Gameplay maintains stable frame pacing under intended conditions.
- The final ROM passes accurate emulation testing.
- Ideally, the same ROM completes a successful run on physical hardware.

## 20. Scope-Cut Order

If time becomes constrained, cut features in this order:

1. Miniboss
2. Fourth enemy
3. Fishbone weapon
4. Hidden rooms and breakable walls
5. Multiple ranks
6. Animated story screens
7. Vertical room
8. Second trash weapon

Never cut the boss, ending, death/restart flow, or hardware-valid ROM. A tiny finished *Trashavania* is the goal.
