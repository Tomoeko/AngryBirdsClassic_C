#ifndef AB_ORACLE_BLUES_SPLIT_H
#define AB_ORACLE_BLUES_SPLIT_H

#include "reference_blues_split.h"

/* Execute original outer/normalizer/settings/puff/class functions. Native
 * world/circle/transform/velocity/collider code is real. GPU puff/renderer,
 * camera sequence, audio-name/play, notification and final parent-removal
 * services are explicit observed boundaries; their implementations are tested
 * separately. No replacement source instructions or original code is shipped. */
static struct {
    void *wrapper;
    const void *parent, *hat, *old_other;
    uint32_t profile, tracing, stages[BS_MAX_STAGES], count;
    uint32_t normalized[2], puff[2], camera[2], audio[2], notify, remove;
} bs_native;

static void bs_record(uint32_t stage) {
    if (!bs_native.tracing)
        return;
    if (bs_native.count == BS_MAX_STAGES)
        fail("Original Blues outer trace exceeded its bound\n");
    bs_native.stages[bs_native.count++] = stage;
}

static void bs_trace(void *lua, uint32_t stage) {
    if (stage == 1)
        bc_native_string(lua, 1, &bc_native.name);
    if (stage == 7)
        bc_native.physical = damage_lua.to_pointer(lua, 1);
    bs_record(stage);
}

static int bs_original(void *lua, const char *name, int outputs) {
    int count = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, name);
    for (int index = 1; index <= count; ++index)
        bc_native.push(lua, index);
    if (damage_lua.call(lua, count, outputs, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        if (message)
            semihost(4, message);
        fail("Original enclosing Blues helper failed\n");
    }
    return outputs;
}

static void bs_name(unsigned profile, unsigned child, BcNativeString *output) {
    const char *prefix = bs_parent_names[profile];
    unsigned length = bc_string_length(prefix);
    for (unsigned index = 0; index < length; ++index)
        output->bytes[index] = prefix[index];
    output->bytes[length] = (char)('a' + child);
    output->bytes[length + 1] = 0;
    output->length = output->capacity = length + 1;
    output->references = UINT32_MAX;
}

static void bs_child(void *lua, unsigned child) {
    BcNativeString name;
    bs_name(bs_native.profile, child, &name);
    bc_native.reference((uint8_t *)bc_native.game + 0x1810);
    damage_lua.get_field(lua, -1, name.bytes);
    void (*remove)(void *, int) =
        (void (*)(void *, int))find_function("_Z10lua_removeP9lua_Statei");
    remove(lua, -2);
}

static int bs_definition(void *lua) {
    if (!damage_named(lua, 1, bs_parent_names[bs_native.profile]))
        fail("Original outer requested another prepared definition\n");
    damage_lua.get_field(lua, -10002, "_ab_bs_definition");
    return 1;
}

static int bs_tutorial(void *lua) {
    bs_record(9);
    return bs_original(lua, "_ab_bs_tutorial", 0);
}

static int bs_transform(void *lua) {
    void (*transform)(FloatBits[2], const void *, float, float) =
        (void (*)(FloatBits[2], const void *, float, float))find_function(
            "_ZNK20CoordinateTransforms23physicsToWorldTransformEff");
    FloatBits result[2];
    transform(result, NULL, damage_lua.to_number(lua, 1), damage_lua.to_number(lua, 2));
    damage_lua.push_number(lua, result[0].number);
    damage_lua.push_number(lua, result[1].number);
    return 2;
}

static int bs_puff_point(void *lua) {
    if (damage_lua.get_top(lua) != 4 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.to_pointer(lua, 2) != bs_native.parent || damage_lua.type(lua, 3) ||
        damage_lua.type(lua, 4))
        fail("Unexpected original Blues puff boundary\n");
    const char *const fields[] = {"x", "y"};
    for (unsigned index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, 1, fields[index]);
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        bs_native.puff[index] = value.bits;
        damage_lua.set_top(lua, 4);
    }
    return 0;
}

static int bs_puff(void *lua) {
    bs_record(10);
    return bs_original(lua, "_ab_bs_puff", 0);
}

static int bs_normalize(void *lua) {
    bs_record(11);
    (void)bs_original(lua, "_ab_bs_normalize", 2);
    for (unsigned index = 0; index < 2; ++index) {
        FloatBits value = {.number = damage_lua.to_number(lua, 3 + (int)index)};
        bs_native.normalized[index] = value.bits;
    }
    return 2;
}

static int bs_teleport(void *lua) {
    bs_record(12);
    BcNativeString name;
    bc_native_string(lua, 1, &name);
    if (!damage_named(lua, 1, bs_parent_names[bs_native.profile]) ||
        damage_lua.type(lua, 2) != 5 || bc_native.length(lua, 2) != 3)
        fail("Unexpected original Blues inheritance arguments\n");
    uint32_t table[5];
    void (*construct)(void *, void *, int) = (void (*)(
        void *, void *, int))find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateEi");
    void (*destroy)(void *) = (void (*)(void *))find_function("_ZN3lua8LuaTableD1Ev");
    void (*inherit)(void *, const char **, void *) =
        (void (*)(void *, const char **, void *))find_function(
            "_ZN7GameLua20inheritTeleportationESsN3lua8LuaTableE");
    construct(table, bs_native.wrapper, 2);
    const char *bytes = name.bytes;
    inherit(bc_native.game, &bytes, table);
    destroy(table);
    return 0;
}

static int bs_add(void *lua) {
    if (damage_named(lua, 2, "MuseumBluesCameraFollowBehavior"))
        bs_record(13);
    return bs_original(lua, "_ab_bs_add", 1);
}

static int bs_get(void *lua) {
    bs_record(14);
    return bs_original(lua, "_ab_bs_get", 1);
}

static int bs_follow(void *lua) {
    ++bc_native.follows;
    return bs_original(lua, "_ab_bs_follow", 0);
}

static int bs_cancel(void *lua) {
    ++bs_native.camera[0];
    /* A prepared actual sequence-end callback may rebind the live global. */
    if (bs_native.profile >= 8 && (bs_native.profile & 1u)) {
        bs_child(lua, 0);
        damage_lua.set_field(lua, -10002, "flyingBird");
    }
    return 0;
}

static int bs_start(void *lua) {
    if (damage_lua.get_top(lua) != 2 ||
        !damage_named(lua, 2, "FollowShotBirdCameraSequence"))
        fail("Original Blues camera requested another sequence\n");
    ++bs_native.camera[1];
    return 0;
}

static int bs_audio_name(void *lua) {
    bs_record(15);
    if (!damage_named(lua, 1, "special_group"))
        fail("Original Blues requested another audio group\n");
    ++bs_native.audio[0];
    /* Deliberately controlled return at the independently native-compared
     * audio-group boundary. Do not claim RNG/audio-device parity here. */
    damage_lua.push_string(lua, "observed_special_choice");
    return 1;
}

static int bs_audio_play(void *lua) {
    bs_record(16);
    if (damage_lua.get_top(lua) != 3 ||
        !damage_named(lua, 1, "observed_special_choice") ||
        damage_lua.to_number(lua, 2) != 1 || damage_lua.type(lua, 3) != 1 ||
        damage_lua.to_boolean(lua, 3))
        fail("Unexpected original Blues audio forwarding\n");
    ++bs_native.audio[1];
    return 0;
}

static int bs_notify(void *lua) {
    if (!damage_named(lua, 1, "EID_BIRD_SPECIALTY_TRIGGERED"))
        fail("Unexpected original Blues notification\n");
    ++bs_native.notify;
    return 0;
}

static int bs_remove(void *lua) {
    if (damage_lua.to_pointer(lua, 1) != bs_native.parent)
        fail("Original outer removed a different parent\n");
    ++bs_native.remove;
    return 0;
}

static void bs_method(void *lua, const char *path, const char *name) {
    load_selected_function(lua, path, "_ab_bs_method");
    damage_lua.get_field(lua, -10002, "settingsWrapper");
    damage_lua.get_field(lua, -10002, "_ab_bs_method");
    damage_lua.set_field(lua, 1, name);
    damage_lua.set_top(lua, 0);
}

static void bs_graph(void *lua) {
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/bs-trajectories.lua");
    /* This class captures its trajectory-table identity at module load. */
    load_damage_chunk(lua, "/data/local/tmp/ab520/bc-blue.lua");
    damage_lua.get_field(lua, -10002, "BehaviorList");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "MuseumBluesTrajectoryRenderer");
    damage_lua.set_field(lua, 2, "class");
    damage_lua.set_field(lua, 1, "MuseumBluesTrajectoryRenderer");
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/bs-outer.lua", "_ab_bs_outer");
    load_selected_function(lua, "/data/local/tmp/ab520/bs-normalize.lua",
                           "_ab_bs_normalize");
    load_selected_function(lua, "/data/local/tmp/ab520/bs-puff.lua", "_ab_bs_puff");
    load_selected_function(lua, "/data/local/tmp/ab520/bs-tutorial.lua",
                           "_ab_bs_tutorial");
    damage_lua.create_table(lua, 0, 3);
    damage_function(lua, 1, "setExtraTutorialShown", bs_tutorial);
    damage_lua.set_field(lua, -10002, "settingsWrapper");
    bs_method(lua, "/data/local/tmp/ab520/bs-get-tutorials.lua", "getTutorials");
    bs_method(lua, "/data/local/tmp/ab520/bs-get-item.lua", "getTutorialsForItem");
    load_damage_chunk(lua, "/data/local/tmp/ab520/bs-camera.lua");
    damage_lua.get_field(lua, -10002, "BehaviorList");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "MuseumBluesCameraFollowBehavior");
    damage_lua.set_field(lua, 2, "class");
    damage_lua.set_field(lua, 1, "MuseumBluesCameraFollowBehavior");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "GameObject");
    damage_lua.get_field(lua, 1, "addBehavior");
    damage_lua.set_field(lua, -10002, "_ab_bs_add");
    damage_lua.get_field(lua, 1, "getBehavior");
    damage_lua.set_field(lua, -10002, "_ab_bs_get");
    damage_function(lua, 1, "addBehavior", bs_add);
    damage_function(lua, 1, "getBehavior", bs_get);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "TrajectoryRenderer");
    damage_lua.get_field(lua, 1, "addObjectToFollow");
    damage_lua.set_field(lua, -10002, "_ab_bs_follow");
    damage_function(lua, 1, "addObjectToFollow", bs_follow);
    damage_function(lua, 1, "addPuffToObject", bs_puff);
    damage_function(lua, 1, "addPuff", bs_puff_point);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "cancelCameraSequence", bs_cancel);
    damage_function(lua, 1, "startCameraSequenceFromFile", bs_start);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    const char *const functions[] = {"vNormalize",           "physicsToWorldTransform",
                                     "inheritTeleportation", "getObjectDefinition",
                                     "getAudioName",         "playAudio",
                                     "notifyEventManager",   "removeBird"};
    int (*callbacks[])(void *) = {bs_normalize,  bs_transform,  bs_teleport,
                                  bs_definition, bs_audio_name, bs_audio_play,
                                  bs_notify,     bs_remove};
    for (unsigned index = 0; index < 8; ++index)
        damage_function(lua, -10002, functions[index], callbacks[index]);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "EID_BIRD_SPECIALTY_TRIGGERED");
    damage_lua.set_field(lua, 1, "EID_BIRD_SPECIALTY_TRIGGERED");
    damage_lua.set_field(lua, -10002, "events");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "blockTable");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "otherBirds");
}

static void bs_prepare(void *lua, unsigned profile) {
    bs_native.profile = profile;
    bs_native.count = bs_native.camera[0] = bs_native.camera[1] = 0;
    bs_native.audio[0] = bs_native.audio[1] = bs_native.notify = bs_native.remove = 0;
    bc_native.follows = 0;
    bc_native.parent_name = bs_parent_names[profile];
    damage_lua.create_table(lua, 0, 4);
    damage_lua.push_string(lua, "CLUSTER_BOMB");
    damage_lua.set_field(lua, 1, "specialty");
    damage_lua.push_string(lua, "BIRD_BLUE");
    damage_lua.set_field(lua, 1, "sprite");
    damage_lua.push_string(lua, "special_group");
    damage_lua.set_field(lua, 1, "specialSound");
    bc_native.push(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_bs_definition");
    damage_lua.get_field(lua, -10002, "blockTable");
    damage_lua.create_table(lua, 0, 1);
    bc_native.push(lua, 1);
    damage_lua.set_field(lua, 3, "SmallBlueBird");
    damage_lua.set_field(lua, 2, "blocks");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "otherBirds");
    bs_native.old_other = damage_lua.to_pointer(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_bs_old_other");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    BcValue help = profile % 4 == 0   ? (BcValue){0}
                   : profile % 4 == 1 ? (BcValue){1, 0}
                   : profile % 4 == 2 ? (BcValue){1, 1}
                                      : (BcValue){3, 0};
    bc_push(lua, help);
    damage_lua.set_field(lua, 3, "showHelp");
    damage_lua.set_field(lua, 2, "BIRD_BLUE");
    damage_lua.set_field(lua, 1, "tutorials");
    damage_lua.set_field(lua, -10002, "settings");
    BcValue parent[BC_PARENT_FIELDS];
    bs_case(profile, parent);
    damage_lua.create_table(lua, 0, BC_PARENT_FIELDS + 3);
    bs_native.parent = damage_lua.to_pointer(lua, 1);
    for (unsigned index = 0; index < BC_PARENT_FIELDS; ++index) {
        if (index == 25)
            damage_lua.push_string(lua, bc_native.parent_name);
        else
            bc_push(lua, parent[index]);
        damage_lua.set_field(lua, 1, bc_parent_fields[index]);
    }
    bc_native.push(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_bs_parent");
    damage_lua.get_field(lua, -10002, "_ab_bc_original_create");
    bc_native.push(lua, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 5, "isCluster");
    damage_lua.set_field(lua, 4, "MuseumBluesTrajectoryRenderer");
    if (damage_lua.call(lua, 2, 0, 0))
        fail("Original prepared cluster class failed\n");
    damage_lua.get_field(lua, 1, "gameObjectScript");
    damage_lua.get_field(lua, 2, "onLaunch");
    bc_native.push(lua, 2);
    if (damage_lua.call(lua, 1, 0, 0))
        fail("Original prepared cluster launch failed\n");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    bs_native.hat = NULL;
    if (profile & 1u) {
        damage_lua.create_table(lua, 0, 0);
        bs_native.hat = damage_lua.to_pointer(lua, 2);
        damage_lua.set_field(lua, 1, bc_native.parent_name);
    }
    damage_lua.set_field(lua, -10002, "g_birdHats");
}

static unsigned bs_role(void *lua, int index) {
    const void *value = damage_lua.to_pointer(lua, index);
    for (unsigned child = 0; child < 3; ++child) {
        int top = damage_lua.get_top(lua);
        bs_child(lua, child);
        int same = value && damage_lua.to_pointer(lua, -1) == value;
        damage_lua.set_top(lua, top);
        if (same)
            return child + 1;
    }
    return 0;
}

static void bs_summary(void *lua, uint32_t output[BS_SUMMARY_WORDS]) {
    for (unsigned index = 0; index < BS_SUMMARY_WORDS; ++index)
        output[index] = 0;
    output[0] = bs_native.normalized[0];
    output[1] = bs_native.normalized[1];
    output[2] = bs_native.puff[0];
    output[3] = bs_native.puff[1];
    damage_lua.get_field(lua, -10002, "settings");
    damage_lua.get_field(lua, 1, "tutorials");
    damage_lua.get_field(lua, 2, "BIRD_BLUE");
    damage_lua.get_field(lua, 3, "showHelp");
    BcValue help = bc_encode(lua, 4);
    output[4] = help.kind;
    output[5] = help.bits;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "g_birdHats");
    damage_lua.get_field(lua, 1, bc_native.parent_name);
    output[6] = damage_lua.type(lua, 2) == 0;
    damage_lua.set_top(lua, 1);
    BcNativeString name;
    bs_name(bs_native.profile, 2, &name);
    damage_lua.get_field(lua, 1, name.bytes);
    output[7] = bs_native.hat && damage_lua.to_pointer(lua, 2) == bs_native.hat;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "otherBirds");
    output[8] = damage_lua.to_pointer(lua, 1) != bs_native.old_other;
    output[9] = bc_native.length(lua, 1);
    void (*get_index)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    output[10] = 1;
    for (unsigned child = 0; child < 2; ++child) {
        get_index(lua, 1, (int)child + 1);
        bs_name(bs_native.profile, child, &name);
        output[10] &= damage_named(lua, 2, name.bytes) != 0;
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "flyingBird");
    output[11] = bs_role(lua, 1);
    damage_lua.set_top(lua, 0);
    for (unsigned child = 0; child < 3; ++child) {
        bs_child(lua, child);
        damage_lua.get_field(lua, 1, "gameObjectScript");
        damage_lua.get_field(lua, 2, "behaviors");
        damage_lua.get_field(lua, 3, "MuseumBluesTrajectoryRenderer");
        get_index(lua, 4, 1);
        damage_lua.get_field(lua, 5, "originalCluster");
        if (damage_lua.to_pointer(lua, 6) == bs_native.parent)
            output[12] = child + 1;
        damage_lua.set_top(lua, 0);
    }
    output[13] = bs_native.camera[0];
    output[14] = bs_native.camera[1];
    output[15] = bs_native.audio[0];
    output[16] = bs_native.audio[1];
    output[17] = bs_native.notify;
    output[18] = bs_native.remove;
    damage_lua.get_field(lua, -10002, "birdSpecialtyAvailable");
    BcValue available = bc_encode(lua, 1);
    output[19] = available.kind;
    output[20] = available.bits;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "birdSpecialty");
    output[21] = damage_named(lua, 1, "CLUSTER_BOMB") != 0;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_bs_parent");
    damage_lua.get_field(lua, 1, "specialtyTriggered");
    BcValue triggered = bc_encode(lua, 2);
    output[22] = triggered.kind;
    output[23] = triggered.bits;
    damage_lua.set_top(lua, 0);
}

static void bs_execute(void *lua, void *wrapper) {
    bs_native.wrapper = wrapper;
    bs_graph(lua);
    semihost(4, "AB_ANDROID_BLUES_SPLIT_V1 16 3\n");
    for (unsigned profile = 0; profile < BS_PROFILES; ++profile) {
        bs_prepare(lua, profile);
        bs_native.tracing = 1;
        damage_lua.get_field(lua, -10002, "_ab_bs_outer");
        damage_lua.get_field(lua, -10002, "_ab_bs_parent");
        damage_lua.push_number(lua, 0);
        if (damage_lua.call(lua, 2, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            if (message)
                semihost(4, message);
            fail("Unchanged original Blues outer branch failed\n");
        }
        bs_native.tracing = 0;
        uint32_t header[] = {profile, bs_native.count, bc_native.follows};
        print_words(header, 3);
        print_words(bs_native.stages, bs_native.count);
        uint32_t summary[BS_SUMMARY_WORDS];
        bs_summary(lua, summary);
        print_words(summary, BS_SUMMARY_WORDS);
        for (unsigned child = 0; child < 3; ++child) {
            bs_name(profile, child, &bc_native.name);
            uint32_t snapshot[BC_SNAPSHOT_WORDS];
            bc_snapshot(lua, snapshot);
            print_words(snapshot, BC_SNAPSHOT_WORDS);
        }
    }
}

static void bs_cleanup(void (*remove_object)(void *, const char **)) {
    for (unsigned profile = 0; profile < BS_PROFILES; ++profile)
        for (unsigned child = 0; child < 3; ++child) {
            BcNativeString storage;
            bs_name(profile, child, &storage);
            const char *name = storage.bytes;
            remove_object(bc_native.game, &name);
        }
}

#endif
