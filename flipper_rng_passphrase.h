#pragma once

#include <stdint.h>
#include <stddef.h>

// Forward declaration - check if already defined
#ifndef FLIPPER_RNG_STATE_DEFINED
struct FlipperRngState;
typedef struct FlipperRngState FlipperRngState;
#endif

// Passphrase configuration
#define PASSPHRASE_MIN_WORDS 3
#define PASSPHRASE_MAX_WORDS 12
#define PASSPHRASE_DEFAULT_WORDS 6

// Using a subset of the EFF short wordlist #1 (most common, short words)
// This list contains 1296 words (6^4) for 4 dice rolls per word
// Each word provides ~10.3 bits of entropy
#define PASSPHRASE_WORDLIST_SIZE 1848  // Temporary - will load full list from SD

// EFF Short Wordlist #1 - optimized for memorability and typing
// Source: https://www.eff.org/dice
static const char* const passphrase_wordlist[PASSPHRASE_WORDLIST_SIZE] = {
    // 1111-1166 (first 54 words)
    "acid", "acorn", "acre", "acts", "afar", "affix",
    "aged", "agent", "agile", "aging", "agony", "ahead",
    "aide", "aids", "aim", "ajar", "alarm", "album",
    "alert", "algae", "alibi", "alien", "alike", "alive",
    "alley", "allow", "alloy", "aloft", "alone", "along",
    "aloof", "aloud", "alpha", "altar", "alter", "amaze",
    "amber", "amend", "amino", "ample", "amuse", "angel",
    "anger", "angle", "angry", "ankle", "annex", "annoy",
    "annul", "anode", "antic", "anvil", "apart", "aphid",
    
    // 1211-1266 (next 54 words)
    "apnea", "apple", "apply", "apron", "aptly", "aqua",
    "area", "arena", "argue", "arise", "armed", "armor",
    "aroma", "arose", "array", "arrow", "arson", "artsy",
    "ascot", "ashen", "ashes", "aside", "asked", "aspen",
    "asset", "atlas", "atom", "atone", "attic", "audio",
    "audit", "augur", "aunt", "aura", "auto", "avert",
    "avoid", "awake", "award", "aware", "awash", "awful",
    "awoke", "awry", "axis", "axle", "aztec", "azure",
    "baby", "bacon", "badge", "badly", "bagel", "baggy",
    
    // 1311-1366 (next 54 words)
    "baker", "balmy", "banjo", "barge", "barn", "baron",
    "barrel", "basic", "basil", "basin", "basis", "batch",
    "bath", "baton", "bats", "blade", "blame", "blank",
    "blast", "blaze", "bleak", "blend", "bless", "blimp",
    "blind", "blink", "blip", "bliss", "blitz", "bloat",
    "blob", "block", "blond", "blood", "bloom", "blown",
    "blues", "bluff", "blunt", "blurb", "blurt", "blush",
    "boast", "boat", "body", "bogey", "boil", "bok",
    "bolt", "bomb", "bone", "boney", "bongo", "bonus",
    
    // 1411-1466
    "boost", "booth", "boots", "boozy", "borax", "bore",
    "born", "boss", "botch", "both", "bound", "bounty",
    "bovine", "bowel", "boxer", "brain", "brake", "brand",
    "brass", "brave", "bread", "break", "breed", "brew",
    "brick", "bride", "brief", "bright", "bring", "brink",
    "briny", "brisk", "broad", "broil", "broke", "brook",
    "broom", "broth", "brown", "browser", "brunt", "brush",
    "buck", "buddy", "budget", "buggy", "bugle", "built",
    "bulge", "bulk", "bull", "bully", "bunch", "bunny",
    
    // 1511-1566
    "bunt", "buoy", "burden", "bureau", "burial", "burn",
    "burnt", "burst", "bury", "bush", "bust", "busy",
    "buyer", "buzz", "cable", "cache", "cadet", "cage",
    "cake", "calm", "camel", "camo", "camp", "canal",
    "candy", "cane", "canon", "canyon", "cape", "card",
    "cargo", "carol", "carry", "carve", "case", "cash",
    "casino", "cast", "cat", "catch", "cater", "cause",
    "cave", "cedar", "cell", "chain", "chair", "chalk",
    "champ", "chant", "chaos", "chaps", "charm", "chart",
    
    // 1611-1666
    "chase", "cheap", "cheat", "check", "cheek", "cheer",
    "chef", "chess", "chest", "chew", "chief", "child",
    "chili", "chill", "chimp", "china", "chip", "chirp",
    "chive", "chock", "choir", "choke", "chomp", "chop",
    "chose", "chow", "chuck", "chug", "chunk", "churn",
    "chute", "cider", "cigar", "cinch", "circa", "cite",
    "city", "civic", "civil", "clad", "claim", "clam",
    "clamp", "clang", "clank", "clap", "clash", "clasp",
    "class", "claw", "clay", "clean", "clear", "cleat",
    
    // 2111-2166
    "cleft", "clerk", "click", "cliff", "climb", "cling",
    "clink", "clip", "cloak", "clock", "clone", "close",
    "cloth", "cloud", "clout", "clove", "clown", "club",
    "cluck", "clue", "clump", "clung", "clunk", "coach",
    "coast", "coat", "cobra", "cocoa", "code", "coil",
    "coke", "cola", "cold", "colon", "color", "colt",
    "coma", "comb", "come", "comic", "comma", "cone",
    "cope", "copy", "coral", "cord", "cork", "corn",
    "cost", "couch", "cough", "could", "count", "coupe",
    
    // 2211-2266
    "court", "cousin", "cover", "covet", "cow", "coy",
    "crab", "crack", "craft", "cramp", "crane", "crank",
    "crash", "crate", "crave", "crawl", "crazy", "creak",
    "cream", "creek", "creep", "crept", "crest", "crew",
    "crib", "cried", "crime", "crisp", "croak", "crock",
    "crook", "crop", "cross", "crow", "crowd", "crown",
    "crud", "crude", "cruel", "crumb", "crush", "crust",
    "cub", "cube", "cubic", "cue", "cuff", "cull",
    "cult", "cupid", "curb", "cure", "curl", "curry",
    
    // 2311-2366
    "curse", "curve", "curvy", "cushy", "cusp", "cut",
    "cycle", "dab", "dad", "daft", "daily", "dairy",
    "daisy", "dam", "dance", "dandy", "dang", "dank",
    "dare", "dark", "darn", "dart", "dash", "data",
    "date", "dawn", "deaf", "deal", "dean", "dear",
    "death", "debit", "debt", "debug", "decaf", "decay",
    "deck", "decor", "decoy", "deed", "deep", "deer",
    "deity", "delay", "delta", "delve", "demon", "denim",
    "dense", "dent", "depth", "derby", "desk", "dial",
    
    // 2411-2466
    "diary", "dice", "dicey", "diced", "diet", "digit",
    "dingy", "diode", "dire", "dirt", "disco", "dish",
    "disk", "ditch", "ditto", "ditzy", "diver", "dizzy",
    "dock", "dodge", "dodgy", "doily", "doing", "doll",
    "dolly", "domain", "dome", "donor", "donut", "doom",
    "door", "dope", "dose", "dot", "doubt", "dough",
    "dove", "down", "dowry", "dozen", "drab", "draft",
    "drag", "drain", "drake", "drama", "drank", "drape",
    "draw", "dread", "dream", "dress", "drew", "dried",
    
    // 2511-2566
    "drift", "drill", "drink", "drive", "droit", "droll",
    "drone", "drool", "droop", "drop", "drown", "drug",
    "drum", "drunk", "dry", "duck", "duct", "dude",
    "dug", "duke", "duly", "dumb", "dummy", "dump",
    "dune", "dung", "dunk", "duo", "dupe", "duration",
    "dusk", "dust", "duty", "dwarf", "dwell", "dyer",
    "dying", "each", "eager", "eagle", "early", "earn",
    "earth", "ease", "easel", "east", "easy", "eaten",
    "eater", "ebay", "ebony", "ebook", "echo", "eclipse",
    
    // 2611-2666
    "edge", "edgy", "edit", "eel", "eerie", "egg",
    "ego", "eight", "eject", "elbow", "elder", "elect",
    "elegy", "elf", "elk", "elm", "elope", "elude",
    "elves", "email", "ember", "emblem", "embryo", "emerge",
    "emit", "emoji", "empty", "enable", "enact", "end",
    "enemy", "energy", "engine", "enjoy", "enlist", "enough",
    "enrage", "ensure", "enter", "entry", "envoy", "enzyme",
    "epic", "epoch", "equal", "equip", "erase", "error",
    "erupt", "essay", "etch", "ethics", "evade", "even",
    
    // 3111-3166
    "event", "every", "evict", "evil", "evoke", "exact",
    "exam", "exceed", "excel", "excess", "excuse", "exit",
    "exile", "exist", "exotic", "expand", "expect", "expert",
    "expire", "export", "expose", "extra", "eye", "fabric",
    "face", "fact", "factor", "fade", "fail", "faint",
    "fair", "fairy", "faith", "fake", "fall", "false",
    "fame", "family", "famine", "fancy", "fang", "far",
    "farm", "fast", "fat", "fate", "father", "fault",
    "fauna", "favor", "fawn", "fax", "fear", "feast",
    
    // 3211-3266
    "feat", "fed", "fee", "feed", "feel", "feet",
    "fell", "felt", "femur", "fence", "fend", "ferry",
    "fest", "fetch", "fetish", "feudal", "fever", "few",
    "fiat", "fiber", "fiction", "fiddle", "field", "fiend",
    "fiery", "fifth", "fifty", "fig", "fight", "figure",
    "file", "fill", "film", "filter", "filth", "final",
    "finch", "find", "fine", "finger", "finish", "finite",
    "fir", "fire", "firm", "first", "fish", "fit",
    "five", "fix", "fizz", "flag", "flail", "flair",
    
    // 3311-3366
    "flake", "flame", "flank", "flap", "flare", "flash",
    "flask", "flat", "flaw", "flax", "fled", "flee",
    "fleet", "flesh", "flew", "flex", "flick", "flier",
    "fling", "flint", "flip", "flirt", "float", "flock",
    "flood", "floor", "flop", "flora", "floss", "flour",
    "flow", "flower", "flu", "flub", "flue", "fluid",
    "fluke", "flung", "flunk", "flush", "flute", "flux",
    "fly", "foal", "foam", "foe", "fog", "foil",
    "foist", "fold", "folk", "folly", "font", "food",
    
    // 3411-3466
    "fool", "foot", "for", "forbid", "force", "ford",
    "fore", "forest", "forge", "forget", "fork", "form",
    "fort", "forth", "forty", "forum", "fossil", "foster",
    "fought", "foul", "found", "four", "fowl", "fox",
    "foyer", "frail", "frame", "frank", "fraud", "fray",
    "freak", "free", "freed", "freeze", "french", "fresh",
    "fret", "friar", "fried", "friend", "frill", "fringe",
    "frisk", "from", "front", "frost", "froth", "frown",
    "froze", "fruit", "fry", "fudge", "fuel", "full",
    
    // 3511-3566
    "fully", "fume", "fun", "fund", "fungi", "funky",
    "funny", "fur", "furry", "fury", "fuse", "fuss",
    "fuzzy", "gag", "gain", "gait", "gala", "galaxy",
    "gale", "gall", "game", "gamma", "gap", "garage",
    "garb", "garden", "garlic", "gas", "gasp", "gate",
    "gather", "gauge", "gave", "gawk", "gaze", "gear",
    "gecko", "geek", "gem", "gender", "gene", "genre",
    "gent", "genus", "get", "geyser", "ghost", "giant",
    "gift", "gig", "gill", "gimp", "gin", "ginger",
    
    // 3611-3666
    "gird", "girl", "gist", "give", "given", "giver",
    "glad", "glade", "glance", "gland", "glare", "glass",
    "glaze", "gleam", "glean", "glee", "glen", "glide",
    "glint", "gloat", "globe", "gloom", "glory", "gloss",
    "glove", "glow", "glue", "glug", "glum", "gnat",
    "gnaw", "gnome", "goal", "goat", "god", "goes",
    "going", "gold", "golf", "gone", "gong", "good",
    "gooey", "goofy", "gore", "gory", "gosh", "got",
    "gouge", "gout", "gown", "grab", "grace", "grade",
    
    // 4111-4166
    "grain", "grand", "grant", "grape", "graph", "grasp",
    "grass", "grate", "grave", "gravy", "gray", "graze",
    "great", "greed", "green", "greet", "grew", "grey",
    "grid", "grief", "grill", "grim", "grime", "grin",
    "grind", "grip", "grit", "groan", "groom", "grope",
    "gross", "group", "grove", "grow", "growl", "grown",
    "grub", "gruff", "grunt", "guard", "guess", "guest",
    "guide", "guild", "guilt", "guise", "gulf", "gull",
    "gulp", "gum", "gummy", "gun", "guru", "gush",
    
    // 4211-4266
    "gust", "gusto", "gut", "guy", "gypsy", "habit",
    "hack", "had", "hag", "hail", "hair", "half",
    "hall", "halt", "ham", "hammer", "hamper", "hand",
    "handy", "hang", "happy", "harbor", "hard", "hardy",
    "hare", "hark", "harm", "harp", "harsh", "has",
    "hash", "hassle", "haste", "hasty", "hat", "hatch",
    "hate", "haul", "haunt", "have", "haven", "havoc",
    "hawk", "hay", "hazard", "haze", "hazel", "hazy",
    
    // 4311-4366
    "head", "heal", "heap", "hear", "heard", "heart",
    "heat", "heave", "heavy", "hedge", "heel", "hefty",
    "height", "helix", "hell", "hello", "helm", "helmet",
    "help", "hem", "hemp", "hence", "henna", "her",
    "herald", "herb", "herd", "here", "hero", "hertz",
    "hex", "hey", "hiatus", "hid", "hidden", "hide",
    "high", "hike", "hill", "hilt", "him", "hind",
    "hinge", "hint", "hip", "hippo", "hire", "his",
    
    // 4411-4466
    "hiss", "hit", "hitch", "hive", "hoagie", "hoard",
    "hoax", "hobby", "hockey", "hog", "hoist", "hold",
    "hole", "holly", "holy", "home", "homing", "honest",
    "honey", "honk", "honor", "hood", "hoof", "hook",
    "hoop", "hop", "hope", "horn", "horror", "horse",
    "host", "hot", "hotel", "hound", "hour", "house",
    "hover", "how", "howl", "hub", "huddle", "hue",
    "huff", "hug", "huge", "hulk", "hull", "hum",
    
    // 4511-4566
    "human", "humid", "humor", "hump", "hunch", "hung",
    "hunk", "hunt", "hurdle", "hurl", "hurray", "hurry",
    "hurt", "hush", "husky", "hut", "hybrid", "hydrant",
    "hymn", "hyper", "ice", "icing", "icon", "icy",
    "idea", "ideal", "idiom", "idiot", "idle", "idol",
    "igloo", "ignore", "iguana", "ill", "image", "imbibe",
    "immune", "impact", "impair", "impala", "impart", "impel",
    "import", "impose", "impress", "improve", "impulse", "inch",
    
    // 4611-4666
    "incite", "income", "incur", "index", "indoor", "induce",
    "inept", "inert", "infant", "infect", "infer", "infirm",
    "influx", "inform", "inhale", "inject", "injure", "ink",
    "inlay", "inlet", "inmate", "inn", "inner", "input",
    "insane", "insect", "insert", "inside", "insist", "inspire",
    "install", "instant", "instead", "instep", "insult", "intact",
    "intake", "intend", "intent", "inter", "into", "invade",
    "invent", "invest", "invite", "invoke", "inward", "ion",
    
    // 5111-5166
    "iota", "iris", "iron", "ironic", "irony", "island",
    "issue", "itch", "item", "itself", "ivory", "ivy",
    "jab", "jacket", "jade", "jag", "jaguar", "jail",
    "jam", "janitor", "jar", "jargon", "jaw", "jazz",
    "jealous", "jeans", "jeep", "jeer", "jelly", "jet",
    "jewel", "jiffy", "jig", "jigsaw", "jingle", "jinx",
    "job", "jockey", "jog", "join", "joint", "joke",
    "jolly", "jolt", "jostle", "jot", "journal", "joy",
    "judge", "jug", "juice", "juicy", "july", "jumbo",
    
    // 5211-5266
    "jump", "jumpy", "june", "jungle", "junior", "junk",
    "junky", "juror", "jury", "just", "justice", "justify",
    "jut", "kale", "kangaroo", "karate", "karma", "kayak",
    "kebab", "keen", "keep", "keg", "kelp", "kennel",
    "kept", "kernel", "kettle", "key", "khaki", "kick",
    "kid", "kidney", "kill", "kiln", "kilo", "kilt",
    "kin", "kind", "kindle", "king", "kink", "kinky",
    "kiosk", "kiss", "kit", "kitchen", "kite", "kitten",
    
    // 5311-5366
    "kitty", "kiwi", "knee", "kneel", "knew", "knife",
    "knight", "knit", "knob", "knock", "knot", "know",
    "koala", "kooky", "kudos", "kung", "lab", "label",
    "labor", "lace", "lack", "lad", "ladder", "laden",
    "ladle", "lady", "lag", "lair", "lake", "lamb",
    "lame", "lamp", "lance", "land", "lane", "lanky",
    "lap", "lapel", "lapse", "laptop", "large", "lark",
    "larva", "laser", "lash", "lasso", "last", "latch",
    
    // 5411-5466
    "late", "lather", "latin", "latter", "laugh", "launch",
    "lava", "law", "lawn", "lawsuit", "lawyer", "lax",
    "lay", "layer", "lazy", "lead", "leader", "leaf",
    "league", "leak", "lean", "leap", "learn", "lease",
    "leash", "least", "leave", "led", "ledge", "leech",
    "leek", "leer", "left", "leg", "legal", "legend",
    "legion", "lego", "lemon", "lend", "length", "lens",
    "lent", "lentil", "leopard", "leper", "less", "lesson",
    
    // 5511-5566
    "let", "lethal", "letter", "lettuce", "level", "lever",
    "levy", "liar", "libel", "liberty", "library", "license",
    "lick", "lid", "lie", "lied", "lien", "lieu",
    "life", "lift", "light", "like", "likely", "lilac",
    "lily", "limb", "lime", "limit", "limp", "line",
    "linen", "liner", "linger", "link", "lint", "lion",
    "lip", "liquid", "liquor", "list", "listen", "lit",
    "liter", "litter", "little", "live", "livid", "living",
    
    // 5611-5666
    "lizard", "llama", "load", "loaf", "loan", "loathe",
    "lobby", "lobe", "local", "lock", "locket", "lodge",
    "loft", "lofty", "log", "logic", "logo", "loin",
    "loiter", "loll", "lone", "long", "look", "loom",
    "loop", "loopy", "loose", "loot", "lope", "lord",
    "lore", "lose", "loser", "loss", "lost", "lot",
    "lotion", "lottery", "lotus", "loud", "lounge", "lousy",
    "love", "lover", "low", "lower", "loyal", "lucid",
    
    // 6111-6166
    "luck", "lucky", "lug", "lull", "lumber", "lumen",
    "lump", "lumpy", "lunar", "lunch", "lung", "lunge",
    "lurch", "lure", "lurid", "lurk", "lush", "lust",
    "lusty", "lute", "luxury", "lying", "lymph", "lynch",
    "lynx", "lyric", "mace", "machine", "macho", "macro",
    "mad", "madam", "made", "madly", "madness", "magazine",
    "magic", "magma", "magnet", "maid", "mail", "maim",
    "main", "major", "make", "maker", "makeup", "male",
    
    // 6211-6266
    "malice", "mall", "malt", "mama", "mambo", "mammal",
    "manage", "mane", "mango", "mangy", "mania", "manic",
    "manner", "manor", "mantel", "mantle", "manual", "many",
    "map", "maple", "marble", "march", "mare", "margin",
    "marine", "mark", "market", "marry", "mars", "marsh",
    "marshal", "mart", "martyr", "marvel", "mascot", "mash",
    "mask", "mason", "mass", "mast", "master", "mat",
    "match", "mate", "material", "math", "matrix", "matter",
    
    // 6311-6366
    "maul", "mauve", "maven", "max", "maybe", "mayor",
    "maze", "mead", "meadow", "meal", "mean", "meant",
    "measure", "meat", "meaty", "medal", "meddle", "media",
    "median", "medic", "medium", "meek", "meet", "meld",
    "melon", "melt", "member", "memo", "memory", "menace",
    "mend", "mental", "mention", "mentor", "menu", "meow",
    "mercy", "mere", "merge", "merit", "merry", "mesh",
    "mess", "message", "messy", "met", "metal", "meter",
    
    // 6411-6466
    "method", "metric", "metro", "mice", "micro", "mid",
    "midday", "middle", "midge", "midway", "might", "mighty",
    "migrate", "mild", "mile", "milk", "milky", "mill",
    "mime", "mimic", "mince", "mind", "mine", "miner",
    "mingle", "mini", "minimal", "minimum", "minion", "mint",
    "minus", "minute", "miracle", "mire", "mirror", "mirth",
    "misery", "misfit", "mishap", "miss", "missile", "mist",
    "mistake", "misty", "miter", "mitten", "mix", "mixer",
    
    // 6511-6566
    "moan", "moat", "mob", "mobile", "mocha", "mock",
    "mode", "model", "modem", "modern", "modest", "modify",
    "module", "moist", "molar", "mold", "moldy", "mole",
    "molt", "molten", "mom", "moment", "monarch", "monday",
    "money", "mongrel", "monitor", "monk", "monkey", "mono",
    "monster", "month", "monument", "mood", "moody", "moon",
    "moor", "moose", "mop", "moral", "more", "morgue",
    "morph", "morsel", "mortal", "mortar", "mosaic", "moses",
    
    // 6611-6666
    "mosquito", "moss", "mossy", "most", "motel", "moth",
    "mother", "motion", "motive", "motor", "motto", "mound",
    "mount", "mourn", "mouse", "mousy", "mouth", "move",
    "movie", "mow", "much", "muck", "mud", "muddy",
    "muffin", "muffle", "mug", "muggy", "mulch", "mule",
    "mull", "multiple", "mummy", "munch", "mural", "murky",
    "murmur", "muscle", "muse", "museum", "mush", "mushy",
    "music", "musk", "musky", "must", "musty", "mute"
};

// Function prototypes
void flipper_rng_passphrase_generate(
    FlipperRngState* state, 
    char* passphrase, 
    size_t max_length, 
    uint8_t num_words);

void flipper_rng_passphrase_generate_sd(
    FlipperRngState* state,
    void* sd_context,  // PassphraseSDContext*
    char* passphrase,
    size_t max_length,
    uint8_t num_words);

uint16_t flipper_rng_passphrase_get_random_index(FlipperRngState* state, uint16_t max_value);

float flipper_rng_passphrase_entropy_bits(uint8_t num_words);
