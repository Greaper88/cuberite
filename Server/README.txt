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

World storage is selected when a world opens. New worlds use the current
writable format, which is legacy Java Anvil with numeric block IDs. This
build refuses to open recognized McRegion, newer Java, and Bedrock worlds.
A legacy region record that fails to load is protected from replacement
even if the server generates temporary terrain in its place.

--------------------------------------------------------------------------------
