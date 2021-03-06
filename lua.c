#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "mace.h"

/* I have used 
   [vis/vis-lua.c](https://github.com/martanne/vis/blob/945db2ed898d4feb55ca9e690dd859503f49c231/vis-lua.c)
   as a reference to figure out how to build an interface for lua.  */

static void
obj_ref_set(lua_State *L, void *addr)
{
  lua_getfield(L, LUA_REGISTRYINDEX, "mace.objects");
  lua_pushlightuserdata(L, addr);
  lua_pushvalue(L, -3);
  lua_settable(L, -3);
  lua_pop(L, 1);
}

static void
obj_type_new(lua_State *L, const char *type)
{
  luaL_newmetatable(L, type);
  lua_getfield(L, LUA_REGISTRYINDEX, "mace.types");
  lua_pushvalue(L, -2);
  lua_pushstring(L, type);
  lua_settable(L, -3);
  lua_pop(L, 1);
}

static void *
obj_ref_new(lua_State *L, void *addr, const char *type)
{
  void **handle;

  if (addr == NULL) {
    lua_pushnil(L);
    return NULL;
  }
  
  lua_getfield(L, LUA_REGISTRYINDEX, "mace.objects");
  lua_pushlightuserdata(L, addr);
  lua_gettable(L, -2);
  lua_remove(L, -2);

  if (lua_type(L, -1) == LUA_TUSERDATA) {
    handle = luaL_checkudata(L, -1, type);

    if (*handle != addr) {
      fprintf(stderr, "lua handle for %p points to %p!\n",
	      addr, *handle);

      return NULL;
    } else {
      return addr;
    }
    
  } else {
    /* Pop the invalid value */
    lua_pop(L, 1); 

    handle = (void **) lua_newuserdata(L, sizeof(addr));

    luaL_getmetatable(L, type);
    lua_setmetatable(L, -2);

    lua_newtable(L);
    lua_setuservalue(L, -2);
	
    obj_ref_set(L, addr);

    *handle = addr;

    return addr;
  }
}

static void *
obj_ref_get(lua_State *L, void *addr, const char *type)
{
  lua_getfield(L, LUA_REGISTRYINDEX, "mace.objects");
  lua_pushlightuserdata(L, addr);
  lua_gettable(L, -2);
  lua_remove(L, -2);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return NULL;
  } else {
    return luaL_checkudata(L, -1, type);
  }
}

static void *
obj_ref_check(lua_State *L, int index, const char *type)
{
  void **handle = luaL_checkudata(L, index, type);

  /* Need to check the object is still in the registry before using
     it. */

  if (obj_ref_get(L, *handle, type) == NULL) {
    return NULL;
  } else {
    return *handle;
  }
}

static int
indexcommon(lua_State *L)
{
  lua_getmetatable(L, 1);
  lua_pushvalue(L, 2);
  lua_gettable(L, -2);

  if (lua_isnil(L, -1)) {
    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
  }
	
  return 1;
}

static int
newindexcommon(lua_State *L)
{
  lua_getuservalue(L, 1);
  lua_pushvalue(L, 2);
  lua_pushvalue(L, 3);
  lua_settable(L, -3);

  return 0;
}

static int
ltabtostring(lua_State *L)
{
  struct tab *t;

  t = obj_ref_check(L, 1, "mace.tab");

  lua_pushfstring(L, "[tab %s]", t->name);

  return 1;
}

static int
ltabindex(lua_State *L)
{
  struct tab *t = obj_ref_check(L, 1, "mace.tab");
  const char *key;

  if (lua_isstring(L, 2)) {
    key = lua_tostring(L, 2);

    if (strcmp(key, "main") == 0) {
      obj_ref_new(L, t->main, "mace.textbox");
      return 1;
    }

    if (strcmp(key, "action") == 0) {
      obj_ref_new(L, t->action, "mace.textbox");
      return 1;
    }

    if (strcmp(key, "next") == 0) {
      obj_ref_new(L, t->next, "mace.tab");
      return 1;
    }
  }

  return indexcommon(L);
}

static int
ltabnewindex(lua_State *L)
{
  struct tab *n, *t = obj_ref_check(L, 1, "mace.tab");
  const char *key;

  if (lua_isstring(L, 2)) {
    key = lua_tostring(L, 2);

    if (strcmp(key, "next") == 0) {
      n = obj_ref_check(L, 3, "mace.tab");
      t->next = n;
      
      return 0;
    }
  }

  return newindexcommon(L);
}

static const struct luaL_Reg tab_funcs[] = {
  { "__tostring", ltabtostring },
  { "__index",    ltabindex },
  { "__newindex", ltabnewindex },
  { NULL,         NULL },
};

static int
ltextboxtostring(lua_State *L)
{
  struct textbox *t;

  t = obj_ref_check(L, 1, "mace.textbox");
  
  lua_pushfstring(L, "[textbox %d]", t->cursor);

  return 1;
}

static int
ltextboxindex(lua_State *L)
{
  struct textbox *t = obj_ref_check(L, 1, "mace.textbox");
  const char *key;

  if (lua_isstring(L, 2)) {
    key = lua_tostring(L, 2);

    if (strcmp(key, "sequence") == 0) {
      t->sequence->lua = L;
      obj_ref_new(L, t->sequence, "mace.sequence");
      return 1;
    }

    if (strcmp(key, "cursor") == 0) {
      lua_pushnumber(L, t->cursor);
      return 1;
    }

    if (strcmp(key, "yoff") == 0) {
      lua_pushnumber(L, t->yoff);
      return 1;
    }

    if (strcmp(key, "tab") == 0) {
      obj_ref_new(L, t->tab, "mace.tab");
      return 1;
    }

    if (strcmp(key, "selections") == 0) {
      obj_ref_new(L, t->selections, "mace.selection");
      return 1;
    }
  }

  return indexcommon(L);
}

static int
ltextboxnewindex(lua_State *L)
{
  struct textbox *t = obj_ref_check(L, 1, "mace.textbox");
  const char *key;
  int i;

  if (lua_isstring(L, 2)) {
    key = lua_tostring(L, 2);

    if (strcmp(key, "cursor") == 0) {
      i = luaL_checknumber(L, 3);
      t->cursor = i;
      textboxpredraw(t);
      return 0;
    }

    if (strcmp(key, "yoff") == 0) {
      i = luaL_checknumber(L, 3);
      t->yoff = i;
      textboxfindstart(t);
      textboxpredraw(t);
      return 0;
    }
  }
  
  return newindexcommon(L);
}

static const struct luaL_Reg textbox_funcs[] = {
  { "__tostring", ltextboxtostring },
  { "__index",    ltextboxindex },
  { "__newindex", ltextboxnewindex },
  { NULL,         NULL },
};

static int
lsequencetostring(lua_State *L)
{
  struct sequence *s;
  
  s = obj_ref_check(L, 1, "mace.sequence");

  lua_pushfstring(L, "[sequence plen %d, dlen %d]", s->plen, s->dlen);

  return 1;
}

static int
lsequenceinsert(lua_State *L)
{
  const uint8_t *data;
  struct sequence *s;
  size_t pos, len;
  bool r;
  
  s = obj_ref_check(L, 1, "mace.sequence");
  pos = luaL_checknumber(L, 2);
  data = (const uint8_t *) luaL_checklstring(L, 3, &len);

  r = sequenceinsert(s, pos, data, len);

  lua_pushboolean(L, r);

  return 1;
}

static int
lsequencedelete(lua_State *L)
{
  struct sequence *s;
  size_t start, len;
  bool r;
  
  s = obj_ref_check(L, 1, "mace.sequence");
  start = luaL_checknumber(L, 2);
  len = luaL_checknumber(L, 3);

  r = sequencedelete(s, start, len);

  lua_pushboolean(L, r);
  
  return 1;
}

static int
lsequenceget(lua_State *L)
{
  size_t start, len;
  struct sequence *s;
  uint8_t *buf;

  s = obj_ref_check(L, 1, "mace.sequence");
  start = luaL_checknumber(L, 2);
  len = luaL_checknumber(L, 3);

  if (len == 0) {
    lua_pushnil(L);
    return 1;
  }
  
  buf = malloc(len);
  if (buf == NULL) {
    lua_pushnil(L);
    return 1;
  }

  len = sequenceget(s, start, buf, len);
  lua_pushlstring(L, (const char *) buf, len);

  free(buf);
  
  return 1;
}

static int
lsequencelen(lua_State *L)
{
  struct sequence *s;

  s = obj_ref_check(L, 1, "mace.sequence");

  lua_pushnumber(L, sequencegetlen(s));

  return 1;
}

static const struct luaL_Reg sequence_funcs[] = {
  { "__tostring", lsequencetostring },
  { "__index",    indexcommon },
  { "__newindex", newindexcommon },
  { "insert",     lsequenceinsert },
  { "delete",     lsequencedelete },
  { "get",        lsequenceget },
  { "len",        lsequencelen },
  { NULL,         NULL },
};

static int
lselectiontostring(lua_State *L)
{
  struct selection *s;

  s = obj_ref_check(L, 1, "mace.selection");

  lua_pushfstring(L, "[selection in %p from %d len %d]",
		  s->textbox, s->start, s->end - s->start + 1);

  return 1;
}

static int
lselectionindex(lua_State *L)
{
  struct selection *s;
  const char *key;
  
  s = obj_ref_check(L, 1, "mace.selection");

  if (lua_isstring(L, 2)) {
    key = lua_tostring(L, 2);

    if (strcmp(key, "next") == 0) {
      obj_ref_new(L, s->next, "mace.selection");
      return 1;
    }

    if (strcmp(key, "start") == 0) {
      lua_pushnumber(L, s->start);
      return 1;
    }

    if (strcmp(key, "len") == 0) {
      lua_pushnumber(L, s->end - s->start + 1);
      return 1;
    }

    if (strcmp(key, "textbox") == 0) {
      obj_ref_new(L, s->textbox, "mace.textbox");
      return 1;
    }
  }

  return indexcommon(L);
}

static int
lselectionnewindex(lua_State *L)
{
  return newindexcommon(L);
}

static const struct luaL_Reg selection_funcs[] = {
  { "__tostring", lselectiontostring },
  { "__index",    lselectionindex },
  { "__newindex", lselectionnewindex },
  { NULL,         NULL },
};

static int
lmacetostring(lua_State *L)
{
  /*struct mace *mace = */obj_ref_check(L, 1, "mace");

  lua_pushfstring(L, "[mace]");

  return 1;
}

static int
lmaceindex(lua_State *L)
{
  struct mace *mace = obj_ref_check(L, 1, "mace");
  const char *key;

  if (lua_isstring(L, 2)) {
    key = lua_tostring(L, 2);

    if (strcmp(key, "focus") == 0) {
      obj_ref_new(L, mace->focus, "mace.textbox");
      return 1;
    }

    if (strcmp(key, "tabs") == 0) {
      obj_ref_new(L, mace->tabs, "mace.tab");
      return 1;
    } 
  }

  return indexcommon(L);
}

static int
lmacenewindex(lua_State *L)
{
  struct mace *mace = obj_ref_check(L, 1, "mace");
  struct textbox *tb;
  struct tab *tab;
  const char *key;
  
  if (lua_isstring(L, 2)) {
    key = lua_tostring(L, 2);

    if (strcmp(key, "focus") == 0) {
      tb = obj_ref_check(L, 3, "mace.textbox");
      mace->focus = tb;
      return 0;
    }

    if (strcmp(key, "tabs") == 0) {
      tab = obj_ref_check(L, 3, "mace.tab");
      mace->tabs = tab;
      return 0;
    }
  }
    
  return newindexcommon(L);
}

static int
lmacesetfont(lua_State *L)
{
  struct mace *mace = obj_ref_check(L, 1, "mace");
  const uint8_t *pattern;
  size_t len;
  int r;

  pattern = (const uint8_t *) luaL_checklstring(L, 2, &len);

  r = fontset(mace->font, pattern);

  lua_pushnumber(L, r);

  return 1;
}

static int
lmacequit(lua_State *L)
{
  struct mace *mace = obj_ref_check(L, 1, "mace");

  macequit(mace);

  return 0;
}

static int
lmaceemptytab(lua_State *L)
{
  struct mace *mace = obj_ref_check(L, 1, "mace");
  const uint8_t *name;
  struct tab *t;
  size_t nlen;

  name = (const uint8_t *) luaL_checklstring(L, 2, &nlen);

  t = tabnewempty(mace, name, nlen);
  if (t == NULL) {
    lua_pushnil(L);
    return 1;
  }

  tabresize(t, mace->focus->tab->x, mace->focus->tab->y,
	    mace->focus->tab->width, mace->focus->tab->height);

  obj_ref_new(L, t, "mace.tab");

  return 1;
}

static int
lmacefiletab(lua_State *L)
{
  struct mace *mace = obj_ref_check(L, 1, "mace");
  const uint8_t *name, *filename;
  size_t nlen, flen;
  struct tab *t;

  name = (const uint8_t *) luaL_checklstring(L, 2, &nlen);
  filename = (const uint8_t *) luaL_checklstring(L, 3, &flen);

  t = tabnewfromfile(mace, name, nlen, filename, flen);
  if (t == NULL) {
    lua_pushnil(L);
    return 1;
  }

  tabresize(t, mace->focus->tab->x, mace->focus->tab->y,
	    mace->focus->tab->width, mace->focus->tab->height);

  obj_ref_new(L, t, "mace.tab");

  return 1;
}

static const struct luaL_Reg mace_funcs[] = {
  { "__tostring",      lmacetostring },
  { "__index",         lmaceindex },
  { "__newindex",      lmacenewindex },
  { "setfont",         lmacesetfont },
  { "quit",            lmacequit },
  { "newemptytab",     lmaceemptytab },
  { "newfiletab",      lmacefiletab },
  { NULL, NULL }
};

void
luaremove(lua_State *L, void *addr)
{
  lua_pushnil(L);
  obj_ref_set(L, addr);
}

void
command(lua_State *L, const uint8_t *s)
{
  lua_getglobal(L, (const char *) s);

  if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
    fprintf(stderr, "error calling %s: %s\n", s, lua_tostring(L, -1));
    return;
  }
}

lua_State *
luanew(struct mace *mace)
{
  lua_State *L;

  L = luaL_newstate();
  if (L == NULL) {
    fprintf(stderr, "Failed to initalize lua!");
    return NULL;
  }

  luaL_openlibs(L);

  lua_newtable(L);
  lua_setfield(L, LUA_REGISTRYINDEX, "mace.types");

  lua_newtable(L);
  lua_setfield(L, LUA_REGISTRYINDEX, "mace.objects");

  obj_type_new(L, "mace.tab");
  luaL_setfuncs(L, tab_funcs, 0);
  
  obj_type_new(L, "mace.textbox");
  luaL_setfuncs(L, textbox_funcs, 0);

  obj_type_new(L, "mace.sequence");
  luaL_setfuncs(L, sequence_funcs, 0);

  obj_type_new(L, "mace.selection");
  luaL_setfuncs(L, selection_funcs, 0);

  obj_type_new(L, "mace");
  luaL_setfuncs(L, mace_funcs, 0);

  obj_ref_new(L, mace, "mace");
  lua_setglobal(L, "mace");

  return L;
}

void
luaruninit(lua_State *L)
{
  int r;

  r = luaL_loadfile(L, "init.lua");
  if (r != LUA_OK) {
    fprintf(stderr, "Error loading init: %s", lua_tostring(L, -1));
    return;
  }

  r = lua_pcall(L, 0, LUA_MULTRET, 0);
  if (r != LUA_OK) {
    fprintf(stderr, "Error running init: %s", lua_tostring(L, -1));
    return;
  }
}

void
luafree(lua_State *L)
{
  lua_close(L);
}
