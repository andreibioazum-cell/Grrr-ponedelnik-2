/* geo_net_parts.inc - /parts.json parser: pushes changed Parts to the inbox. */

/* Parse /parts.json; push only changed Parts into the inbox ring so the
 * game thread applies them without re-spawning every poll. */
static GeoPartNet geo_part_shadow[64];
static int geo_part_shadow_n;
static void geo_parse_parts_json(const char *json, unsigned int gen) {
    const char *p = json;
    if (!p || *p != '{')
        return;
    p++;
    while (*p) {
        char name[24];
        const char *obj_s, *obj_e;
        GeoPartNet pt;
        int i, found = -1;
        while (*p && *p != '"' && *p != '}')
            p++;
        if (*p != '"')
            break;
        p = geo_json_read_str(p + 1, name, sizeof(name));
        while (*p && *p != ':')
            p++;
        if (*p == ':')
            p++;
        while (*p == ' ')
            p++;
        if (*p != '{') {   /* null = deleted elsewhere; handled by absence */
            p = (*p == '[') ? geo_skip_balanced(p) : p + 1;
            continue;
        }
        obj_s = p;
        obj_e = geo_skip_balanced(p);
        p = obj_e;
        memset(&pt, 0, sizeof(pt));
        snprintf(pt.name, sizeof(pt.name), "%s", name);
        pt.x = (float)geo_obj_num(obj_s, obj_e, "x", 0.0);
        pt.y = (float)geo_obj_num(obj_s, obj_e, "y", 0.0);
        pt.z = (float)geo_obj_num(obj_s, obj_e, "z", 0.0);
        pt.sx = (float)geo_obj_num(obj_s, obj_e, "sx", 1.0);
        pt.sy = (float)geo_obj_num(obj_s, obj_e, "sy", 1.0);
        pt.sz = (float)geo_obj_num(obj_s, obj_e, "sz", 1.0);
        pt.yaw = (float)geo_obj_num(obj_s, obj_e, "ry", 0.0);
        pt.block = (int)geo_obj_num(obj_s, obj_e, "b", 1.0);
        for (i = 0; i < geo_part_shadow_n; i++)
            if (strcmp(geo_part_shadow[i].name, name) == 0) {
                found = i;
                break;
            }
        if (found >= 0 && memcmp(&geo_part_shadow[found], &pt, sizeof(pt)) == 0)
            continue;
        if (found >= 0)
            geo_part_shadow[found] = pt;
        else if (geo_part_shadow_n < 64)
            geo_part_shadow[geo_part_shadow_n++] = pt;
        ds_mutex_lock(net_lock);
        if (net.run && net.gen == gen) {
            int next = (net.part_in_head + 1) % 256;
            if (next != net.part_in_tail) {
                net.part_in[net.part_in_head] = pt;
                net.part_in_head = next;
            }
        }
        ds_mutex_unlock(net_lock);
    }
}
