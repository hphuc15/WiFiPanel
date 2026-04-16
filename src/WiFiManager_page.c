#include "WiFiManager.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ===================== INTERNAL: STRING BUILDER ===================== */

/** Growable heap string buffer. Resizes automatically via realloc. */
typedef struct
{
    char *buf;  /**< Heap-allocated character buffer. */
    size_t len; /**< Current string length (excluding null terminator). */
    size_t cap; /**< Current allocated capacity in bytes. */
} StrBuf;

/** Initialize a growable string buffer with initial capacity. */
static int sb_init(StrBuf *sb, size_t initial_cap)
{
    sb->buf = (char *)malloc(initial_cap);
    if (!sb->buf)
        return -1;
    sb->buf[0] = '\0';
    sb->len = 0;
    sb->cap = initial_cap;
    return 0;
}

/** Append string s to the buffer, reallocating if needed. */
static int sb_append(StrBuf *sb, const char *s)
{
    size_t slen = strlen(s);
    size_t need = sb->len + slen + 1;

    if (need > sb->cap)
    {
        size_t new_cap = sb->cap * 2;
        if (new_cap < need)
            new_cap = need;

        char *tmp = (char *)realloc(sb->buf, new_cap);
        if (!tmp)
            return -1;

        sb->buf = tmp;
        sb->cap = new_cap;
    }

    memcpy(sb->buf + sb->len, s, slen + 1);
    sb->len += slen;
    return 0;
}

/** Release the buffer and return the internal string. Caller must free(). */
static char *sb_release(StrBuf *sb)
{
    char *result = sb->buf;
    sb->buf = NULL;
    sb->len = sb->cap = 0;
    return result;
}

/* ===================== INTERNAL: HTML PAGE BUILDER ================== */

/** Escape HTML special chars to prevent XSS. Caller must free(). */
static char *escape_html(const char *s)
{
    size_t len = strlen(s);
    char *out = (char *)malloc(len * 6 + 1);
    if (!out)
        return NULL;

    char *p = out;
    for (; *s; s++)
    {
        switch (*s)
        {
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

/** Append CSS styles to the page buffer. */
static int append_css(StrBuf *sb)
{
    return sb_append(sb,
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
                     "</style>");
}

/** Append extra dynamic input fields to the page buffer. */
static int append_extra_fields(StrBuf *sb, const WiFiManagerPage_t *page)
{
    for (size_t i = 0; i < page->count; i++)
    {
        const WiFiManagerParam_t *p = &page->params[i];

        char *eid  = escape_html(p->id);
        char *elbl = escape_html(p->label);
        char *eph  = escape_html(p->placeholder);
        char *eval = escape_html(p->value);
        char *etyp = escape_html(p->type);
        if (!eid || !elbl || !eph || !eval || !etyp)
        {
            free(eid); free(elbl); free(eph); free(eval); free(etyp);
            return -1;
        }

        const char *req = p->required ? " required" : "";

        sb_append(sb, "<div class=\"form-group\">");
        sb_append(sb, "<label for=\""); sb_append(sb, eid); sb_append(sb, "\">");
        sb_append(sb, elbl); sb_append(sb, "</label>");

        if (strcmp(p->type, "password") == 0)
        {
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
        }
        else
        {
            sb_append(sb, "<input type=\""); sb_append(sb, etyp);
            sb_append(sb, "\" id=\""); sb_append(sb, eid);
            sb_append(sb, "\" name=\""); sb_append(sb, eid);
            sb_append(sb, "\" placeholder=\""); sb_append(sb, eph);
            sb_append(sb, "\" value=\""); sb_append(sb, eval);
            sb_append(sb, "\""); sb_append(sb, req); sb_append(sb, ">");
        }

        if (p->placeholder[0] != '\0')
        {
            sb_append(sb, "<div class=\"label-info\">");
            sb_append(sb, eph);
            sb_append(sb, "</div>");
        }
        sb_append(sb, "</div>");

        free(eid); free(elbl); free(eph); free(eval); free(etyp);
    }
    return 0;
}

/** Append form submit JavaScript to the page buffer. */
static int append_script(StrBuf *sb, const WiFiManagerPage_t *page)
{
    StrBuf required_ids, all_ids;
    if (sb_init(&required_ids, 128) < 0)
        return -1;
    if (sb_init(&all_ids, 128) < 0)
    {
        free(required_ids.buf);
        return -1;
    }

    for (size_t i = 0; i < page->count; i++)
    {
        const WiFiManagerParam_t *p = &page->params[i];
        sb_append(&all_ids, "'"); sb_append(&all_ids, p->id); sb_append(&all_ids, "',");
        if (p->required)
        {
            sb_append(&required_ids, "'"); sb_append(&required_ids, p->id); sb_append(&required_ids, "',");
        }
    }

    sb_append(sb, "<script>"
                  "function toggleField(id){"
                  "var el=document.getElementById(id);"
                  "el.type=el.type==='password'?'text':'password';}"
                  "document.getElementById('configForm').addEventListener('submit',function(e){"
                  "e.preventDefault();"
                  "var ssid=document.getElementById('ssid').value.trim();"
                  "var status=document.getElementById('statusMessage');"
                  "if(!ssid){status.textContent='SSID is required';"
                  "status.className='status error';return;}"
                  "var required=[");
    sb_append(sb, required_ids.buf);
    sb_append(sb, "];"
                  "for(var i=0;i<required.length;i++){"
                  "var el=document.getElementById(required[i]);"
                  "if(el&&!el.value.trim()){status.textContent=required[i]+' is required';"
                  "status.className='status error';return;}}"
                  "var body=new URLSearchParams();"
                  "body.append('ssid',ssid);"
                  "body.append('password',document.getElementById('password').value);"
                  "var extras=[");
    sb_append(sb, all_ids.buf);
    sb_append(sb, "];"
                  "for(var j=0;j<extras.length;j++){"
                  "var el=document.getElementById(extras[j]);"
                  "if(el)body.append(extras[j],el.value);}"
                  "fetch('/',{method:'POST',"
                  "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
                  "body:body.toString()})"
                  ".then(function(res){"
                  "if(!res.ok)throw new Error('Save failed');"
                  "status.textContent='Saved. Restarting...';"
                  "status.className='status success';})"
                  ".catch(function(err){"
                  "status.textContent='Error: '+err.message;"
                  "status.className='status error';});"
                  "});"
                  "</script>");

    free(required_ids.buf);
    free(all_ids.buf);
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
    if (page->count >= WM_MAX_PARAMS){
        return -1;
    }
    WiFiManagerParam_t *p = &page->params[page->count++];
    memset(p, 0, sizeof(*p));

    strncpy(p->id, id ? id : "", WM_FIELD_LEN - 1);
    strncpy(p->label, label ? label : "", WM_FIELD_LEN - 1);
    strncpy(p->placeholder, placeholder ? placeholder : "", WM_FIELD_LEN - 1);
    strncpy(p->value, value ? value : "", WM_FIELD_LEN - 1);
    strncpy(p->type, type ? type : "text", sizeof(p->type) - 1);
    p->required = required;

    return 0;
}

const char *WiFiManagerPage_GetParam(const WiFiManager_t *wm, const char *id)
{
    WiFiManagerPage_t *page = &wm->page;
    for (size_t i = 0; i < page->count; i++)
    {
        if (strcmp(page->params[i].id, id) == 0)
            return page->params[i].value;
    }
    return NULL;
}

char *WiFiManagerPage_Build(const WiFiManager_t *wm)
{
    WiFiManagerPage_t *page = &wm->page;
    StrBuf sb;
    if (sb_init(&sb, 5120) < 0)
        return NULL;

    /* <head> */
    sb_append(&sb, "<!DOCTYPE html><html lang=\"en\"><head>"
                   "<meta charset=\"UTF-8\">"
                   "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">"
                   "<title>WiFi Configuration</title>");
    if (append_css(&sb) < 0)
        goto fail;
    sb_append(&sb, "</head><body><div class=\"container\">"
                   "<header><h1>WiFi Configuration</h1></header>"
                   "<form class=\"config-form\" id=\"configForm\">");

    /* Fixed: SSID */
    sb_append(&sb, "<div class=\"form-group\">"
                   "<label for=\"ssid\">SSID</label>"
                   "<input type=\"text\" id=\"ssid\" name=\"ssid\" required>"
                   "<div class=\"label-info\">WiFi name</div>"
                   "</div>");

    /* Fixed: Password */
    sb_append(&sb, "<div class=\"form-group\">"
                   "<label for=\"password\">Password</label>"
                   "<div class=\"password-wrapper\">"
                   "<input type=\"password\" id=\"password\" name=\"password\">"
                   "<span class=\"toggle-password\""
                   " onclick=\"toggleField('password')\">&#128065;</span>"
                   "</div>"
                   "<div class=\"label-info\">WiFi password</div>"
                   "</div>");

    /* Dynamic fields */
    if (append_extra_fields(&sb, page) < 0)
        goto fail;

    sb_append(&sb, "<button type=\"submit\" class=\"submit-btn\">Save</button>"
                   "<div id=\"statusMessage\" class=\"status\"></div>"
                   "</form></div>");

    if (append_script(&sb, page) < 0)
        goto fail;
    sb_append(&sb, "</body></html>");

    return sb_release(&sb);

fail:
    free(sb.buf);
    return NULL;
}