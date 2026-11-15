--------------------------------------------------------------------------------
   Welcome to your new Cuberite server, compatible with Minecraft 1.8 - 1.12!
--------------------------------------------------------------------------------

 - To get started with your server, read the user's manual at
   https://book.cuberite.org/

 - Subscribe to the Newsletter for update news and important information at
   https://cuberite.org/news/#subscribe

 - For information about adding plugins to your server, visit
   https://cuberite.org/plugins/

 - For additional support, visit https://cuberite.org/discuss/

To offer different Java resource packs by client version, add a [ResourcePacks]
section to settings.ini. Each entry uses an exact protocol ID and contains a
direct download URL followed by the 40-digit SHA-1 of the ZIP file.

    [ResourcePacks]
    47=https://example.org/packs/java-1.8.zip 0123456789abcdef0123456789abcdef01234567
    340=https://example.org/packs/java-1.12.2.zip 89abcdef0123456789abcdef0123456789abcdef

Replace the example URLs and hashes with your own. Clients without an entry
use ResourcePackUrl and the optional ResourcePackSha1 from [Server].
RequireResourcePack in [Server] also applies to the version-specific offers.

Older Java clients automatically receive a supported substitute for missing
blocks. Mining, placement and block behavior use the real block on the
server. Some substitutes differ in shape, color or client-side prediction.

To use the built-in HTTP server, enable WebAdmin and set ResourcePackBaseUrl
in the [Server] section of settings.ini to a URL reachable by players.

    [Server]
    ResourcePackBaseUrl=http://minecraft.example.org:8080

From the source checkout, generate the packs with Python 3 and your local
vanilla Java 1.12.2 client JAR. Set --output to resource-packs beside the
server's settings.ini and restart Cuberite.

    python3 Tools/LegacyResourcePacks/generate.py /path/to/1.12.2.jar --output /path/to/server/resource-packs

Cuberite automatically offers each resource-packs/<protocol>.zip to the
matching Java clients and serves it from a public, content-addressed HTTP
route. Explicit [ResourcePacks] entries override the local ZIP for that
protocol. The WebAdmin pages still require their configured login. If you
use a reverse proxy, route /resource-packs/ to Cuberite's HTTP listener.
To host packs externally, also pass --base-url to the generator and merge
the printed [ResourcePacks] entries into settings.ini.

The 1.8 pack textures purpur blocks and end stone bricks. The 1.8 and 1.9
packs also texture magma, nether wart blocks and red nether bricks. Other
missing blocks, including concrete on 1.11, use vanilla look-alikes. Held
and dropped block items also use look-alikes. Players who skip the pack see
the ordinary substitute models.

Experimental Java 1.15.2 clients can join by setting
EnableExperimentalJava1152=1 in [Server] in settings.ini and restarting.
This adapter runs the existing Cuberite worlds and block set. It supports
chunk and light streaming, movement, inventory, and basic block interactions.
The recipe book, command suggestions, and specialised entity metadata are
incomplete. Java 1.19.4 and display entities are future milestones.

World storage is selected when a world opens. New worlds use Java 1.15.2
Anvil. Existing legacy worlds keep the numeric Anvil format. Java 1.13
through 1.15.2 worlds keep the DataVersion of each existing chunk and use the
level.dat DataVersion for new chunks. The world format is independent of
which Java client versions can connect.

Use [WorldPaths] in settings.ini to point Cuberite at an existing Java world.
Point linked Nether and End worlds at its DIM-1 and DIM1 directories when
importing those dimensions. Cuberite uses the parent level.dat for these
directories when they have no local level.dat.

Named states retain their original properties on disk. Older Java clients
receive compatible blocks, and the simulation uses legacy block behaviors.
Cuberite loads and saves supported mobs, block entities and inventory stacks
in the Java format. Records the engine cannot represent, including entities
with unsupported inventory data, are preserved on disk. Unsupported block
entities are read-only in play. Cuberite keeps its maps, scoreboard and
statistics in a separate cuberite directory inside paletted worlds.

Java 1.15 through 1.15.2 chunks support bees, bee nests, and beehives.
Newly generated Overworld trees can have occupied bee nests. Bees visit
flowers and bring nectar back to their homes. Harvest full nests or hives
with glass bottles or shears. Smoke from a lit campfire calms bees during
harvest. A comparator reads the honey level from 0 to 5. Silk Touch carries
the bees and honey level in the item so they return when it is placed.
Honey levels and occupants survive restarts, including in carried items.

Campfires cook up to four food items at once in Java 1.14 through 1.15.2
chunks. Use a lit campfire with raw food. Each item takes 600 game ticks
and drops when cooked. Food and per-slot progress survive a restart in
native Anvil storage. Older clients see compatible substitutes for newer
blocks, mobs, and items. Older chunks retain their original data version.

Roadmap items include native Bedrock connections over NetherNet, Bedrock
.mcworld compatibility, and datapack support. A Lua plugin is a possible
way to implement a subset of datapack features.

AI assistance disclosure

OpenAI Codex contributed to the implementation and testing of paletted
Anvil entity, block entity and item persistence in this fork, including
bee gameplay, campfire smoke and cooking, comparators, Silk Touch transfers,
and client compatibility.

--------------------------------------------------------------------------------
