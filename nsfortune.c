/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis,WITHOUT WARRANTY OF ANY KIND,either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * Alternatively,the contents of this file may be used under the terms
 * of the GNU General Public License(the "GPL"),in which case the
 * provisions of GPL are applicable instead of those above.  If you wish
 * to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the
 * License,indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above,a recipient may use your
 * version of this file under either the License or the GPL.
 *
 * Author Vlad Seryakov vlad@crystalballinc.com
 * 
 */

#include "ns.h"

typedef struct _NsFortuneOffset {
    int file;
    int size;
    char *text;
    unsigned long offset;
} NsFortuneOffset;

typedef struct _NsFortuneServer {
    const char *path;
    int text_load;
    int line_count;
    int file_count;
    int file_alloc;
    char **file_list;
    int offset_count;
    int offset_alloc;
    int max_size;
    NsFortuneOffset *offset_list;
} NsFortuneServer;

static void NsFortuneAddFile(NsFortuneServer * fortune, char *file);
static void NsFortuneAddPath(NsFortuneServer * fortune, const char *path);
static void NsFortune(NsFortuneServer * fortune, Tcl_Interp * interp);
static int NsFortuneCmd(ClientData, Tcl_Interp *, int, Tcl_Obj * CONST objv[]);

static Ns_TclTraceProc NsFortuneInterpInit;

NS_EXPORT int Ns_ModuleVersion = 1;

NS_EXPORT int Ns_ModuleInit(char *server, char *module)
{
    const char *path;
    NsFortuneServer *fortune = (NsFortuneServer *) ns_calloc(1, sizeof(NsFortuneServer));

    path = Ns_ConfigGetPath(server, module, NULL);

    Ns_ConfigGetBool(path, "line_count", &fortune->line_count);
    Ns_ConfigGetBool(path, "text_load", &fortune->text_load);

    if (!(fortune->path = Ns_ConfigGetValue(path, "path")))
        fortune->path = "/usr/share/games/fortunes";
    NsFortuneAddPath(fortune, fortune->path);

    Ns_Log(Notice, "nsfortune: %s: loaded %d epigrams from %d files",
           fortune->path, fortune->offset_count, fortune->file_count);
    Ns_TclRegisterTrace(server, NsFortuneInterpInit, fortune, NS_TCL_TRACE_CREATE);
    return NS_OK;
}

static int NsFortuneInterpInit(Tcl_Interp * interp, const void *context)
{
    Tcl_CreateObjCommand(interp, "ns_fortune", NsFortuneCmd, (ClientData)context, NULL);
    return NS_OK;
}

static int NsFortuneCmd(ClientData data, Tcl_Interp * interp, int objc, Tcl_Obj * CONST objv[])
{
    NsFortuneServer *fortune = (NsFortuneServer *) data;
    int cmd;
    enum cmds {
        cmdAddFile, cmdAddPath, cmdFortune, cmdStat
    };
    static const char *Cmds[] = {
        "addfile", "addpath", "fortune", "stat", 0
    };

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ...");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], Cmds, "command", TCL_EXACT, (int *) &cmd) != TCL_OK)
        return TCL_ERROR;

    switch (cmd) {
    case cmdStat:
        Ns_Log(Notice, "nsfortune: loaded %d epigrams from %d files", fortune->offset_count, fortune->file_count);
        break;

    case cmdAddFile:
        if (objc > 2)
            NsFortuneAddFile(fortune, Tcl_GetString(objv[2]));
        break;

    case cmdAddPath:
        if (objc > 2)
            NsFortuneAddPath(fortune, Tcl_GetString(objv[2]));
        break;

    case cmdFortune:
        NsFortune(fortune, interp);
        break;
    }
    return TCL_OK;
}

static void NsFortuneAddPath(NsFortuneServer * fortune, const char *path)
{
    DIR *dir;
    char buf[256];
    struct dirent *ent;

    if (!(dir = opendir(path))) {
        Ns_Log(Error, "nsfortune: addpath: %s: %s", path, strerror(errno));
        return;
    }
    while ((ent = readdir(dir))) {
        if (strchr(ent->d_name, '.'))
            continue;
        snprintf(buf, sizeof(buf), "%s/%s", path, ent->d_name);
        NsFortuneAddFile(fortune, buf);
    }
    closedir(dir);
}

static void NsFortuneAddFile(NsFortuneServer * fortune, char *file)
{
    FILE *fp;
    char line[256];
    Ns_DString ds;
    unsigned long offset = 0, line_count = 0;

    if (!(fp = fopen(file, "r"))) {
        Ns_Log(Error, "nsfortune: addfile: %s: %s", file, strerror(errno));
        return;
    }
    if (fortune->file_count + 1 >= fortune->file_alloc) {
        fortune->file_alloc += 100;
        fortune->file_list = (char **) ns_realloc(fortune->file_list, sizeof(char *) * fortune->file_alloc);
    }
    Ns_DStringInit(&ds);
    fortune->file_list[fortune->file_count] = ns_strdup(file);
    while ((fgets(line, sizeof(line), fp))) {
        if (strcmp(line, "%\n")) {
            Ns_DStringAppend(&ds, line);
            line_count++;
            continue;
        }
        if (!fortune->line_count || (fortune->line_count && line_count <= fortune->line_count)) {
            if (fortune->offset_count + 1 >= fortune->offset_alloc) {
                fortune->offset_alloc += 100;
                fortune->offset_list =
                    (NsFortuneOffset *) ns_realloc(fortune->offset_list, sizeof(NsFortuneOffset) * fortune->offset_alloc);
            }
            fortune->offset_list[fortune->offset_count].file = fortune->file_count;
            fortune->offset_list[fortune->offset_count].size = ds.length;
            fortune->offset_list[fortune->offset_count].offset = offset;
            fortune->offset_list[fortune->offset_count].text = fortune->text_load ? ns_strdup(ds.string) : 0;
            fortune->offset_count++;
        }
        offset = ftell(fp);
        Ns_DStringTrunc(&ds, 0);
    }
    fortune->file_count++;
    Ns_DStringFree(&ds);
    fclose(fp);
}

static void NsFortune(NsFortuneServer * fortune, Tcl_Interp * interp)
{
    FILE *fp;
    char *buf;
    int i = (int) (Ns_DRand() * fortune->offset_count);

    if (fortune->offset_list[i].text) {
        Tcl_SetResult(interp, fortune->offset_list[i].text, TCL_STATIC);
        return;
    }

    if (!(fp = fopen(fortune->file_list[fortune->offset_list[i].file], "r")))
        return;
    fseek(fp, fortune->offset_list[i].offset, 0);
    buf = (char *) ns_calloc(1, fortune->offset_list[i].size + 1);
    fread(buf, fortune->offset_list[i].size, 1, fp);
    fclose(fp);
    Tcl_SetResult(interp, buf, (Tcl_FreeProc *) ns_free);
}
