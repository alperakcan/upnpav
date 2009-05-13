
typedef struct metadata_snapshot_s metadata_snapshot_t;

metadata_snapshot_t * metadata_snapshot_init (const char *path, int width, int height);
int metadata_snapshot_uninit (metadata_snapshot_t *snapshot);
int metadata_snapshot_obtain (metadata_snapshot_t *snapshot, unsigned int seconds);
