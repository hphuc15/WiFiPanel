#include "WiFiManager.h"
#include "WiFiManager_defs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ===================== INTERNAL: STRING BUILDER ===================== */

typedef struct
{
    char  *buf;
    size_t len;
    size_t cap;
} StrBuf;

static int sb_init(StrBuf *sb, size_t initial_cap)
{
    sb->buf = (char *)malloc(initial_cap);
    if (!sb->buf) return -1;
    sb->buf[0] = '\0';
    sb->len = 0;
    sb->cap = initial_cap;
    return 0;
}

static int sb_append(StrBuf *sb, const char *s)
{
    if (!sb->buf || !s) return -1;
    size_t slen = strlen(s);
    if (slen > SIZE_MAX - sb->len - 1) return -1;
    size_t need = sb->len + slen + 1;
    if (need > sb->cap) {
        size_t new_cap = sb->cap * 2;
        if (new_cap < need) new_cap = need;
        char *tmp = (char *)realloc(sb->buf, new_cap);
        if (!tmp) return -1;
        sb->buf = tmp;
        sb->cap = new_cap;
    }
    memcpy(sb->buf + sb->len, s, slen + 1);
    sb->len += slen;
    return 0;
}

static char *sb_release(StrBuf *sb)
{
    char *result = sb->buf;
    sb->buf = NULL;
    sb->len = sb->cap = 0;
    return result;
}

/* ===================== STATIC HTML PARTS (flash / .rodata) ========= */

/* HEAD: DOCTYPE + CSS + fixed SSID/Password fields.
 * Dynamic extra fields are chunked in between HEAD and TAIL by the handler.
 * Nothing here is allocated on heap — stored in flash. */
static const char WM_HTML_HEAD[] =
    "<!DOCTYPE html><html lang=\"en\"><head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">"
    "<title>WiFi Configuration</title>"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box;font-family:'Segoe UI',sans-serif}"
    "body{background:#f5f5f5;display:flex;justify-content:center;"
    "align-items:center;min-height:100vh;padding:20px}"
    ".container{background:#fff;border-radius:2px;width:100%;"
    "max-width:450px;padding:30px}"
    "h1{color:#2c3e50;font-size:28px;margin-bottom:20px;text-align:center}"
    ".form-group{margin-bottom:20px}"
    "label{display:block;color:#34495e;font-weight:600;margin-bottom:8px}"
    ".label-info{font-size:12px;color:#7f8c8d;margin-top:5px}"
    "input{width:100%;padding:12px 15px;border:1px solid #ddd;"
    "border-radius:2px;font-size:16px}"
    "input:focus{outline:none;border-color:#3498db}"
    ".password-wrapper{position:relative}"
    ".toggle-password{position:absolute;right:12px;top:50%;"
    "transform:translateY(-50%);cursor:pointer;color:#7f8c8d}"
    ".submit-btn{width:100%;background:#3498db;color:#fff;border:none;"
    "border-radius:2px;padding:14px;font-size:16px;"
    "font-weight:600;cursor:pointer;margin-top:4px}"
    ".submit-btn:hover{background:#2980b9}"
    ".status{margin-top:20px;padding:12px;border-radius:2px;"
    "text-align:center;display:none}"
    ".status.success{background:#d4edda;color:#155724;display:block}"
    ".status.error{background:#f8d7da;color:#721c24;display:block}"
    "</style>"
    "</head><body><div class=\"container\">"
    "<header><h1>WiFi Configuration</h1></header>"
    "<form class=\"config-form\" id=\"configForm\">"
    /* Fixed SSID field */
    "<div class=\"form-group\">"
    "<label for=\"ssid\">SSID</label>"
    "<input type=\"text\" id=\"ssid\" name=\"ssid\" required>"
    "<div class=\"label-info\">WiFi name</div>"
    "</div>"
    /* Fixed Password field */
    "<div class=\"form-group\">"
    "<label for=\"password\">Password</label>"
    "<div class=\"password-wrapper\">"
    "<input type=\"password\" id=\"password\" name=\"password\">"
    "<span class=\"toggle-password\""
    " onclick=\"toggleField('password')\">&#128065;</span>"
    "</div>"
    "<div class=\"label-info\">WiFi password</div>"
    "</div>";

/* TAIL: Submit button + JS (fetch-based POST, no page reload).
 * JS collects all <input name=...> fields automatically — no hardcoded id list needed. */
static const char WM_HTML_TAIL[] =
    "<button type=\"submit\" class=\"submit-btn\">Save</button>"
    "<div id=\"statusMessage\" class=\"status\"></div>"
    "</form></div>"
    "<script>"
    "function toggleField(id){"
    "var el=document.getElementById(id);"
    "el.type=el.type==='password'?'text':'password';}"
    "document.getElementById('configForm').addEventListener('submit',function(e){"
    "e.preventDefault();"
    "var ssid=document.getElementById('ssid').value.trim();"
    "var st=document.getElementById('statusMessage');"
    "if(!ssid){st.textContent='SSID is required';"
    "st.className='status error';return;}"
    "var body=new URLSearchParams();"
    "var inputs=document.getElementById('configForm')"
    ".querySelectorAll('input[name]');"
    "for(var i=0;i<inputs.length;i++)"
    "body.append(inputs[i].name,inputs[i].value);"
    "fetch('/',{method:'POST',"
    "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
    "body:body.toString()})"
    ".then(function(r){"
    "if(!r.ok)throw new Error('Save failed');"
    "st.textContent='Saved. Restarting...';"
    "st.className='status success';})"
    ".catch(function(err){"
    "st.textContent='Error: '+err.message;"
    "st.className='status error';});"
    "});"
    "</script>"
    "</body></html>";

/* ===================== INTERNAL: DYNAMIC FIELDS ==================== */

static char *escape_html(const char *s)
{
    size_t len = strlen(s);
    char *out = (char *)malloc(len * 6 + 1);
    if (!out) return NULL;
    char *p = out;
    for (; *s; s++) {
        switch (*s) {
        case '&':  p += sprintf(p, "&amp;");  break;
        case '<':  p += sprintf(p, "&lt;");   break;
        case '>':  p += sprintf(p, "&gt;");   break;
        case '"':  p += sprintf(p, "&quot;"); break;
        case '\'': p += sprintf(p, "&#39;");  break;
        default:   *p++ = *s;                 break;
        }
    }
    *p = '\0';
    return out;
}

static int append_extra_fields(StrBuf *sb, const WiFiManagerPage_t *page)
{
    for (size_t i = 0; i < page->count; i++) {
        const WiFiManagerParam_t *p = &page->params[i];

        char *eid  = escape_html(p->id);
        char *elbl = escape_html(p->label);
        char *eph  = escape_html(p->placeholder);
        char *eval = escape_html(p->value);
        char *etyp = escape_html(p->type);
        if (!eid || !elbl || !eph || !eval || !etyp) {
            free(eid); free(elbl); free(eph); free(eval); free(etyp);
            return -1;
        }

        const char *req = p->required ? " required" : "";

        sb_append(sb, "<div class=\"form-group\">");
        sb_append(sb, "<label for=\""); sb_append(sb, eid); sb_append(sb, "\">");
        sb_append(sb, elbl); sb_append(sb, "</label>");

        if (strcmp(p->type, "password") == 0) {
            sb_append(sb, "<div class=\"password-wrapper\">"
                          "<input type=\"password\" id=\"");
            sb_append(sb, eid); sb_append(sb, "\" name=\""); sb_append(sb, eid);
            sb_append(sb, "\" placeholder=\""); sb_append(sb, eph);
            sb_append(sb, "\" value=\""); sb_append(sb, eval);
            sb_append(sb, "\""); sb_append(sb, req); sb_append(sb, ">");
            sb_append(sb, "<span class=\"toggle-password\""
                          " onclick=\"toggleField('");
            sb_append(sb, eid);
            sb_append(sb, "')\">&#128065;</span></div>");
        } else {
            sb_append(sb, "<input type=\""); sb_append(sb, etyp);
            sb_append(sb, "\" id=\""); sb_append(sb, eid);
            sb_append(sb, "\" name=\""); sb_append(sb, eid);
            sb_append(sb, "\" placeholder=\""); sb_append(sb, eph);
            sb_append(sb, "\" value=\""); sb_append(sb, eval);
            sb_append(sb, "\""); sb_append(sb, req); sb_append(sb, ">");
        }

        if (p->placeholder[0] != '\0') {
            sb_append(sb, "<div class=\"label-info\">");
            sb_append(sb, eph);
            sb_append(sb, "</div>");
        }
        sb_append(sb, "</div>");

        free(eid); free(elbl); free(eph); free(eval); free(etyp);
    }
    return 0;
}

/* ===================== PUBLIC: PAGE APIs ============================ */

void WiFiManagerPage_Init(WiFiManager_t *wm)
{
    memset(&wm->page, 0, sizeof(wm->page));
}

int WiFiManagerPage_AddParam(WiFiManager_t *wm,
                             const char *id,
                             const char *label,
                             const char *placeholder,
                             const char *value,
                             const char *type,
                             bool required)
{
    WiFiManagerPage_t *page = &wm->page;
    if (page->count >= WM_MAX_PARAMS) return -1;
    WiFiManagerParam_t *p = &page->params[page->count++];
    memset(p, 0, sizeof(*p));

    strncpy(p->id,          id ? id : "",                  WM_FIELD_LEN - 1);
    strncpy(p->label,       label ? label : "",             WM_FIELD_LEN - 1);
    strncpy(p->placeholder, placeholder ? placeholder : "", WM_FIELD_LEN - 1);
    strncpy(p->value,       value ? value : "",             WM_FIELD_LEN - 1);
    strncpy(p->type,        type ? type : "text",           sizeof(p->type) - 1);
    p->required = required;
    return 0;
}

const char *WiFiManagerPage_GetParam(const WiFiManager_t *wm, const char *id)
{
    const WiFiManagerPage_t *page = &wm->page;
    for (size_t i = 0; i < page->count; i++) {
        if (strcmp(page->params[i].id, id) == 0)
            return page->params[i].value;
    }
    return NULL;
}

/* Build only the dynamic extra fields — small alloc (~300 bytes).
 * Caller must free(). Returns NULL on failure. */
char *WiFiManagerPage_BuildFields(const WiFiManager_t *wm)
{
    StrBuf sb;
    if (sb_init(&sb, 512) < 0) return NULL;
    if (append_extra_fields(&sb, &wm->page) < 0) {
        free(sb.buf);
        return NULL;
    }
    return sb_release(&sb);
}

const char *WiFiManagerPage_GetHead(void) { return WM_HTML_HEAD; }
const char *WiFiManagerPage_GetTail(void) { return WM_HTML_TAIL; }