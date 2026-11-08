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

--------------------------------------------------------------------------------
