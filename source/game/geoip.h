#ifdef HAVE_MAXMINDDB
#include <maxminddb.h>

MMDB_s *mmdb = NULL;
#endif

string _geoip_filename;

struct customflag
{
    char code[MAXCOUNTRYCODELEN+1];
    string name;
};
vector<customflag> customflags;

ICOMMAND(clearcustomflags, "", (), { customflags.shrink(0); });
ICOMMAND(addcustomflag, "ss", (char *code, char *name), {
    customflag f;
    copystring(f.code, code, MAXCOUNTRYCODELEN);
    copystring(f.name, name, MAXSTRLEN);
    customflags.add(f);
});

void geoip_close_database()
{
    #ifdef HAVE_MAXMINDDB
    if(!mmdb) return;
    MMDB_close(mmdb);
    free(mmdb);
    mmdb = NULL;
    #endif
}

void geoip_open_database(int geoip)
{
    #ifdef HAVE_MAXMINDDB
    if(mmdb) geoip_close_database();
    if(!geoip || !_geoip_filename[0]) return;

    mmdb = (MMDB_s *)calloc(1, sizeof(MMDB_s));
    if(!mmdb) return;

    int status = MMDB_open(_geoip_filename, MMDB_MODE_MMAP, mmdb);
    if(MMDB_SUCCESS != status)
    {
        free(mmdb);
        mmdb = NULL;
    }
    #endif
}

void geoip_lookup_ip(enet_uint32 ip, char *dst_country_code, char *dst_country_name)
{
    dst_country_code[0] = '\0';
    dst_country_name[0] = '\0';

    #ifdef HAVE_MAXMINDDB
    static string text;
    if(!mmdb) return;
    int error;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    memcpy(&sin.sin_addr, &ip, sizeof(enet_uint32));

    MMDB_lookup_result_s result = MMDB_lookup_sockaddr(mmdb, (struct sockaddr *)&sin, &error);
    if(MMDB_SUCCESS == error && result.found_entry)
    {
        MMDB_entry_data_s data;

        // get country code
        error = MMDB_get_value(&result.entry, &data, "country", "iso_code", NULL);
        if(MMDB_SUCCESS == error && data.has_data && MMDB_DATA_TYPE_UTF8_STRING == data.type && data.data_size >= 2)
        {
            copystring(text, data.utf8_string, data.data_size+1);
            filtertext(dst_country_code, text, T_NONE, MAXCOUNTRYCODELEN);
        }

        // get country name
        error = MMDB_get_value(&result.entry, &data, "country", "names", "en", NULL);
        if(MMDB_SUCCESS == error && data.has_data && MMDB_DATA_TYPE_UTF8_STRING == data.type)
        {
            copystring(text, data.utf8_string, data.data_size <= MAXSTRLEN ? data.data_size : MAXSTRLEN);
            filtertext(dst_country_name, text, T_WHITESPACE | T_FORCESPACE, MAXSTRLEN);
        }
    }
    #endif
}

void geoip_set_custom_flag(const char *preferred_flag, const char *src_country_code, const char *src_country_name, char *dst_country_code, char *dst_country_name)
{
    dst_country_code[0] = '\0';
    dst_country_name[0] = '\0';

    if(!strcmp("geo", preferred_flag))
    {
        // client wants the flag of its country
        copystring(dst_country_code, src_country_code, MAXCOUNTRYCODELEN);
        copystring(dst_country_name, src_country_name, MAXSTRLEN);
        return;
    }
    // check if the client wants a custom flag
    if(preferred_flag[0]) loopv(customflags)
    {
        if(!strcmp(preferred_flag, customflags[i].code))
        {
            copystring(dst_country_code, customflags[i].code, MAXCOUNTRYCODELEN);
            copystring(dst_country_name, customflags[i].name, MAXSTRLEN);
            return;
        }
    }
}

VARF(geoip, 0, 0, 1, geoip_open_database(geoip));
SVARF(geoip_database, "geoip.mmdb", {
    copystring(_geoip_filename, geoip_database, MAXSTRLEN);
    geoip_open_database(geoip);
});