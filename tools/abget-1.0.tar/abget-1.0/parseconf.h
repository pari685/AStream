#ifndef __PARSECONF_H
#define __PARSECONF_H

typedef struct conf_parameter {
  char* name;
  char* value;
  struct conf_parameter *next;
} conf_parameter_t;

typedef struct conf_category_entry {
  conf_parameter_t *params;
  struct conf_category_entry *next;
} conf_category_entry_t;

int pc_load(char* filelist);
char* pc_get_param(conf_category_entry_t *entry, char* name);
conf_category_entry_t* pc_get_category(char *cat);
int pc_close();
#endif
