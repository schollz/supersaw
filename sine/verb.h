struct sDattorroVerb;

/* Get pointer to initialized DattorroVerb struct */
struct sDattorroVerb* DattorroVerb_create(void);

/* Free resources and delete DattorroVerb instance */
void DattorroVerb_delete(struct sDattorroVerb* v);

/* Set reverb parameters */
void DattorroVerb_setPreDelay(struct sDattorroVerb* v, float value);
void DattorroVerb_setPreFilter(struct sDattorroVerb* v, float value);
void DattorroVerb_setInputDiffusion1(struct sDattorroVerb* v, float value);
void DattorroVerb_setInputDiffusion2(struct sDattorroVerb* v, float value);
void DattorroVerb_setDecayDiffusion(struct sDattorroVerb* v, float value);
void DattorroVerb_setDecay(struct sDattorroVerb* v, float value);
void DattorroVerb_setDamping(struct sDattorroVerb* v, float value);

/* Send mono input into reverbation tank */
void DattorroVerb_process(struct sDattorroVerb* v, float in);

/* Get reverbated signal for left channel */
float DattorroVerb_getLeft(struct sDattorroVerb* v);

/* Get reverbated signal for right channel */
float DattorroVerb_getRight(struct sDattorroVerb* v);
