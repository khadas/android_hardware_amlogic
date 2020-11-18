/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "TSPMessage"
//#define LOG_NDEBUG 0
//#define DUMP_STATS

#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "TSPMessage.h"
#include "tsp_platform.h"
#include "TSPLooperRoster.h"
#include "TSPHandler.h"

extern TSPLooperRoster gTsPlayerLooperRoster;

dp_state TSPReplyToken::setReply(const sp<TSPMessage> &reply) {
    if (mReplied) {
        ALOGE("trying to post a duplicate reply");
        return -EBUSY;
    }
    if (mReply != NULL) {
        ALOGE("setReply return no memory");
        return -NO_MEMORY;
    }

    mReply = reply;
    mReplied = true;
    return OK;
}
TSPMessage::TSPMessage(void)
    : mWhat(0),
      mTarget(0),
      mNumItems(0) {
}

TSPMessage::TSPMessage(uint32_t what, const sp<const TSPHandler> &handler)
    : mWhat(what),
      mNumItems(0) {
    setTarget(handler);
}

TSPMessage::~TSPMessage() {
    clear();
}

void TSPMessage::setWhat(uint32_t what) {
    mWhat = what;
}

uint32_t TSPMessage::what() const {
    return mWhat;
}

void TSPMessage::setTarget(const sp<const TSPHandler> &handler) {
    if (handler == NULL) {
        mTarget = 0;
        mHandler.clear();
        mLooper.clear();
    } else {
        mTarget = handler->id();
        mHandler = handler->getHandler();
        mLooper = handler->getLooper();
    }
}

void TSPMessage::clear() {
    for (size_t i = 0; i < mNumItems; ++i) {
        Item *item = &mItems[i];
        delete[] item->mName;
        item->mName = NULL;
        freeItemValue(item);
    }
    mNumItems = 0;
}

void TSPMessage::freeItemValue(Item *item) {
    switch (item->mType) {
        case kTypeString:
        {
            free(item->u.stringValue);
            break;
        }

        case kTypeObject:
        case kTypeMessage:
        case kTypeBuffer:
        {
            if (item->u.refValue != NULL) {
                item->u.refValue->decStrong(this);
            }
            break;
        }

        default:
            break;
    }
    item->mType = kTypeInt32; // clear type
}

#ifdef DUMP_STATS
#include <Mutex.h>

TSPMutex gLock;
static int32_t gFindItemCalls = 1;
static int32_t gDupCalls = 1;
static int32_t gAverageNumItems = 0;
static int32_t gAverageNumChecks = 0;
static int32_t gAverageNumMemChecks = 0;
static int32_t gAverageDupItems = 0;
static int32_t gLastChecked = -1;

static void reportStats() {
    int32_t time = (TSPLooper::GetNowUs() / 1000);
    if (time / 1000 != gLastChecked / 1000) {
        gLastChecked = time;
        ALOGI("called findItemIx %zu times (for len=%.1f i=%.1f/%.1f mem) dup %zu times (for len=%.1f)",
                gFindItemCalls,
                gAverageNumItems / (float)gFindItemCalls,
                gAverageNumChecks / (float)gFindItemCalls,
                gAverageNumMemChecks / (float)gFindItemCalls,
                gDupCalls,
                gAverageDupItems / (float)gDupCalls);
        gFindItemCalls = gDupCalls = 1;
        gAverageNumItems = gAverageNumChecks = gAverageNumMemChecks = gAverageDupItems = 0;
        gLastChecked = time;
    }
}
#endif

inline size_t TSPMessage::findItemIndex(const char *name, size_t len) const {
#ifdef DUMP_STATS
    size_t memchecks = 0;
#endif
    size_t i = 0;
    for (; i < mNumItems; i++) {
        if (len != mItems[i].mNameLength) {
            continue;
        }
#ifdef DUMP_STATS
        ++memchecks;
#endif
        if (!memcmp(mItems[i].mName, name, len)) {
            break;
        }
    }
#ifdef DUMP_STATS
    {
        TSPMutex::Autolock _l(gLock);
        ++gFindItemCalls;
        gAverageNumItems += mNumItems;
        gAverageNumMemChecks += memchecks;
        gAverageNumChecks += i;
        reportStats();
    }
#endif
    return i;
}

// assumes item's name was uninitialized or NULL
void TSPMessage::Item::setName(const char *name, size_t len) {
    mNameLength = len;
    mName = new char[len + 1];
    memcpy((void*)mName, name, len + 1);
}

TSPMessage::Item *TSPMessage::allocateItem(const char *name) {
    size_t len = strlen(name);
    size_t i = findItemIndex(name, len);
    Item *item;

    if (i < mNumItems) {
        item = &mItems[i];
        freeItemValue(item);
    } else {
        i = mNumItems++;
        item = &mItems[i];
        item->mType = kTypeInt32;
        item->setName(name, len);
    }

    return item;
}

const TSPMessage::Item *TSPMessage::findItem(
        const char *name, Type type) const {
    size_t i = findItemIndex(name, strlen(name));
    if (i < mNumItems) {
        const Item *item = &mItems[i];
        return item->mType == type ? item : NULL;

    }
    return NULL;
}

bool TSPMessage::findAsFloat(const char *name, float *value) const {
    size_t i = findItemIndex(name, strlen(name));
    if (i < mNumItems) {
        const Item *item = &mItems[i];
        switch (item->mType) {
            case kTypeFloat:
                *value = item->u.floatValue;
                return true;
            case kTypeDouble:
                *value = (float)item->u.doubleValue;
                return true;
            case kTypeInt64:
                *value = (float)item->u.int64Value;
                return true;
            case kTypeInt32:
                *value = (float)item->u.int32Value;
                return true;
            case kTypeSize:
                *value = (float)item->u.sizeValue;
                return true;
            default:
                return false;
        }
    }
    return false;
}

bool TSPMessage::findAsInt64(const char *name, int64_t *value) const {
    size_t i = findItemIndex(name, strlen(name));
    if (i < mNumItems) {
        const Item *item = &mItems[i];
        switch (item->mType) {
            case kTypeInt64:
                *value = item->u.int64Value;
                return true;
            case kTypeInt32:
                *value = item->u.int32Value;
                return true;
            default:
                return false;
        }
    }
    return false;
}

bool TSPMessage::contains(const char *name) const {
    size_t i = findItemIndex(name, strlen(name));
    return i < mNumItems;
}

#define BASIC_TYPE(NAME,FIELDNAME,TYPENAME)                             \
void TSPMessage::set##NAME(const char *name, TYPENAME value) {            \
    Item *item = allocateItem(name);                                    \
                                                                        \
    item->mType = kType##NAME;                                          \
    item->u.FIELDNAME = value;                                          \
}                                                                       \
                                                                        \
/* NOLINT added to avoid incorrect warning/fix from clang.tidy */       \
bool TSPMessage::find##NAME(const char *name, TYPENAME *value) const {  /* NOLINT */ \
    const Item *item = findItem(name, kType##NAME);                     \
    if (item) {                                                         \
        *value = item->u.FIELDNAME;                                     \
        return true;                                                    \
    }                                                                   \
    return false;                                                       \
}

BASIC_TYPE(Int32,int32Value,int32_t)
BASIC_TYPE(Int64,int64Value,int64_t)
BASIC_TYPE(Size,sizeValue,size_t)
BASIC_TYPE(Float,floatValue,float)
BASIC_TYPE(Double,doubleValue,double)
BASIC_TYPE(Pointer,ptrValue,void *)

#undef BASIC_TYPE

void TSPMessage::setString(
        const char *name, const char *s, ssize_t len) {
    Item *item = allocateItem(name);
    item->mType = kTypeString;
    item->u.stringValue = (char *)malloc(sizeof(char) * len);
	strncpy(item->u.stringValue, s, len);
}

void TSPMessage::setObjectInternal(
        const char *name, const sp<RefBase> &obj, Type type) {
    Item *item = allocateItem(name);
    item->mType = type;

    if (obj != NULL) { obj->incStrong(this); }
    item->u.refValue = obj.get();
}

void TSPMessage::setObject(const char *name, const sp<RefBase> &obj) {
    setObjectInternal(name, obj, kTypeObject);
}

void TSPMessage::setMessage(const char *name, const sp<TSPMessage> &obj) {
    Item *item = allocateItem(name);
    item->mType = kTypeMessage;

    if (obj != NULL) { obj->incStrong(this); }
    item->u.refValue = obj.get();
}

void TSPMessage::setRect(
        const char *name,
        int32_t left, int32_t top, int32_t right, int32_t bottom) {
    Item *item = allocateItem(name);
    item->mType = kTypeRect;

    item->u.rectValue.mLeft = left;
    item->u.rectValue.mTop = top;
    item->u.rectValue.mRight = right;
    item->u.rectValue.mBottom = bottom;
}

bool TSPMessage::findString(const char *name, const char **value) const {
    const Item *item = findItem(name, kTypeString);
    if (item) {
        *value = (const char*)(item->u.stringValue);
        return true;
    }
    return false;
}

bool TSPMessage::findObject(const char *name, sp<RefBase> *obj) const {
    const Item *item = findItem(name, kTypeObject);
    if (item) {
        *obj = item->u.refValue;
        return true;
    }
    return false;
}

bool TSPMessage::findMessage(const char *name, sp<TSPMessage> *obj) const {
    const Item *item = findItem(name, kTypeMessage);
    if (item) {
        *obj = static_cast<TSPMessage *>(item->u.refValue);
        return true;
    }
    return false;
}

bool TSPMessage::findRect(
        const char *name,
        int32_t *left, int32_t *top, int32_t *right, int32_t *bottom) const {
    const Item *item = findItem(name, kTypeRect);
    if (item == NULL) {
        return false;
    }

    *left = item->u.rectValue.mLeft;
    *top = item->u.rectValue.mTop;
    *right = item->u.rectValue.mRight;
    *bottom = item->u.rectValue.mBottom;

    return true;
}

void TSPMessage::deliver() {
    sp<TSPHandler> handler = mHandler.promote();
    if (handler == NULL) {
        ALOGW("failed to deliver message as target handler %d is gone.", mTarget);
        return;
    }

    handler->deliverMessage(this);
}

dp_state TSPMessage::post(int64_t delayUs) {
    sp<TSPLooper> looper = mLooper.promote();
    if (looper == NULL) {
        ALOGW("failed to post message as target looper for handler %d is gone.", mTarget);
        return -ENOENT;
    }

    looper->post(this, delayUs);
    return OK;
}

dp_state TSPMessage::postAndAwaitResponse(sp<TSPMessage> *response) {
    sp<TSPLooper> looper = mLooper.promote();
    if (looper == NULL) {
        ALOGW("failed to post message as target looper for handler %d is gone.", mTarget);
        return -ENOENT;
    }

    sp<TSPReplyToken> token = looper->createReplyToken();
    if (token == NULL) {
        ALOGE("failed to create reply token");
        return -ENOMEM;
    }
    setObject("replyID", token);

    looper->post(this, 0 /* delayUs */);
    return looper->awaitResponse(token, response);
}
dp_state TSPMessage::postReply(const sp<TSPReplyToken> &replyToken) {
    if (replyToken == NULL) {
        ALOGW("failed to post reply to a NULL token");
        return -ENOENT;
    }
    sp<TSPLooper> looper = replyToken->getLooper();
    if (looper == NULL) {
        ALOGW("failed to post reply as target looper is gone.");
        return -ENOENT;
    }
    return looper->postReply(replyToken, this);
}

bool TSPMessage::senderAwaitsResponse(sp<TSPReplyToken> *replyToken) {
    sp<RefBase> tmp;
    bool found = findObject("replyID", &tmp);

    if (!found) {
        return false;
    }

    *replyToken = static_cast<TSPReplyToken *>(tmp.get());
    tmp.clear();
    setObject("replyID", tmp);
    // TODO: delete Object instead of setting it to NULL

    return *replyToken != NULL;
}
sp<TSPMessage> TSPMessage::dup() const {
    sp<TSPMessage> msg = new TSPMessage(mWhat, mHandler.promote());
    msg->mNumItems = mNumItems;

#ifdef DUMP_STATS
    {
        TSPMutex::Autolock _l(gLock);
        ++gDupCalls;
        gAverageDupItems += mNumItems;
        reportStats();
    }
#endif

    for (size_t i = 0; i < mNumItems; ++i) {
        const Item *from = &mItems[i];
        Item *to = &msg->mItems[i];

        to->setName(from->mName, from->mNameLength);
        to->mType = from->mType;

        switch (from->mType) {
            case kTypeString:
            {
                to->u.stringValue = (char *)malloc(strlen(from->u.stringValue));
	            strncpy(to->u.stringValue, from->u.stringValue, strlen(from->u.stringValue));
                break;
            }

            case kTypeObject:
            case kTypeBuffer:
            {
                to->u.refValue = from->u.refValue;
                to->u.refValue->incStrong(msg.get());
                break;
            }

            case kTypeMessage:
            {
                sp<TSPMessage> copy =
                    static_cast<TSPMessage *>(from->u.refValue)->dup();

                to->u.refValue = copy.get();
                to->u.refValue->incStrong(msg.get());
                break;
            }

            default:
            {
                to->u = from->u;
                break;
            }
        }
    }

    return msg;
}

sp<TSPMessage> TSPMessage::changesFrom(const sp<const TSPMessage> &other, bool deep) const {
    if (other == NULL) {
        return const_cast<TSPMessage*>(this);
    }

    sp<TSPMessage> diff = new TSPMessage;
    if (mWhat != other->mWhat) {
        diff->setWhat(mWhat);
    }
    if (mHandler != other->mHandler) {
        diff->setTarget(mHandler.promote());
    }

    for (size_t i = 0; i < mNumItems; ++i) {
        const Item &item = mItems[i];
        const Item *oitem = other->findItem(item.mName, item.mType);
        switch (item.mType) {
            case kTypeInt32:
                if (oitem == NULL || item.u.int32Value != oitem->u.int32Value) {
                    diff->setInt32(item.mName, item.u.int32Value);
                }
                break;

            case kTypeInt64:
                if (oitem == NULL || item.u.int64Value != oitem->u.int64Value) {
                    diff->setInt64(item.mName, item.u.int64Value);
                }
                break;

            case kTypeSize:
                if (oitem == NULL || item.u.sizeValue != oitem->u.sizeValue) {
                    diff->setSize(item.mName, item.u.sizeValue);
                }
                break;

            case kTypeFloat:
                if (oitem == NULL || item.u.floatValue != oitem->u.floatValue) {
                    diff->setFloat(item.mName, item.u.sizeValue);
                }
                break;

            case kTypeDouble:
                if (oitem == NULL || item.u.doubleValue != oitem->u.doubleValue) {
                    diff->setDouble(item.mName, item.u.sizeValue);
                }
                break;

            case kTypeString:
                if (oitem == NULL || strcmp(item.u.stringValue,oitem->u.stringValue) != 0) {
                    diff->setString(item.mName, item.u.stringValue);
                }
                break;

            case kTypeRect:
                if (oitem == NULL || memcmp(&item.u.rectValue, &oitem->u.rectValue, sizeof(Rect))) {
                    diff->setRect(
                            item.mName, item.u.rectValue.mLeft, item.u.rectValue.mTop,
                            item.u.rectValue.mRight, item.u.rectValue.mBottom);
                }
                break;

            case kTypePointer:
                if (oitem == NULL || item.u.ptrValue != oitem->u.ptrValue) {
                    diff->setPointer(item.mName, item.u.ptrValue);
                }
                break;

            case kTypeMessage:
            {
                sp<TSPMessage> myMsg = static_cast<TSPMessage *>(item.u.refValue);
                if (myMsg == NULL) {
                    if (oitem == NULL || oitem->u.refValue != NULL) {
                        diff->setMessage(item.mName, NULL);
                    }
                    break;
                }
                sp<TSPMessage> oMsg =
                    oitem == NULL ? NULL : static_cast<TSPMessage *>(oitem->u.refValue);
                sp<TSPMessage> changes = myMsg->changesFrom(oMsg, deep);
                if (changes->countEntries()) {
                    diff->setMessage(item.mName, deep ? changes : myMsg);
                }
                break;
            }

            case kTypeObject:
                if (oitem == NULL || item.u.refValue != oitem->u.refValue) {
                    diff->setObject(item.mName, item.u.refValue);
                }
                break;

            default:
            {
                ALOGE("Unknown type %d", item.mType);
            }
        }
    }
    return diff;
}

size_t TSPMessage::countEntries() const {
    return mNumItems;
}

const char *TSPMessage::getEntryNameAt(size_t index, Type *type) const {
    if (index >= mNumItems) {
        *type = kTypeInt32;

        return NULL;
    }

    *type = mItems[index].mType;

    return mItems[index].mName;
}

TSPMessage::ItemData TSPMessage::getEntryAt(size_t index) const {
    ItemData it;
    if (index < mNumItems) {
        switch (mItems[index].mType) {
            case kTypeInt32:    it.set(mItems[index].u.int32Value); break;
            case kTypeInt64:    it.set(mItems[index].u.int64Value); break;
            case kTypeSize:     it.set(mItems[index].u.sizeValue); break;
            case kTypeFloat:    it.set(mItems[index].u.floatValue); break;
            case kTypeDouble:   it.set(mItems[index].u.doubleValue); break;
            case kTypePointer:  it.set(mItems[index].u.ptrValue); break;
            case kTypeRect:     it.set(mItems[index].u.rectValue); break;
            case kTypeString:   it.set(mItems[index].u.stringValue); break;
            case kTypeObject: {
                sp<RefBase> obj = mItems[index].u.refValue;
                it.set(obj);
                break;
            }
            case kTypeMessage: {
                sp<TSPMessage> msg = static_cast<TSPMessage *>(mItems[index].u.refValue);
                it.set(msg);
                break;
            }
            default:
                break;
        }
    }
    return it;
}

dp_state TSPMessage::setEntryNameAt(size_t index, const char *name) {
    if (index >= mNumItems) {
        return BAD_INDEX;
    }
    if (name == nullptr) {
        return BAD_VALUE;
    }
    if (!strcmp(name, mItems[index].mName)) {
        return OK; // name has not changed
    }
    size_t len = strlen(name);
    if (findItemIndex(name, len) < mNumItems) {
        return ALREADY_EXISTS;
    }
    delete[] mItems[index].mName;
    mItems[index].mName = nullptr;
    mItems[index].setName(name, len);
    return OK;
}

dp_state TSPMessage::setEntryAt(size_t index, const ItemData &item) {
    sp<RefBase> refValue;
    sp<TSPMessage> msgValue;
	char *stringValue = NULL;

    if (index >= mNumItems) {
        return BAD_INDEX;
    }
    if (!item.used()) {
        return BAD_VALUE;
    }
    Item *dst = &mItems[index];
    freeItemValue(dst);

    // some values can be directly set with the getter. others need items to be allocated
    if (item.find(&dst->u.int32Value)) {
        dst->mType = kTypeInt32;
    } else if (item.find(&dst->u.int64Value)) {
        dst->mType = kTypeInt64;
    } else if (item.find(&dst->u.sizeValue)) {
        dst->mType = kTypeSize;
    } else if (item.find(&dst->u.floatValue)) {
        dst->mType = kTypeFloat;
    } else if (item.find(&dst->u.doubleValue)) {
        dst->mType = kTypeDouble;
    } else if (item.find(&dst->u.ptrValue)) {
        dst->mType = kTypePointer;
    } else if (item.find(&dst->u.rectValue)) {
        dst->mType = kTypeRect;
    } else if (item.find(&stringValue)) {
        dst->u.stringValue = (char*) malloc(strlen(stringValue));
		strncpy(dst->u.stringValue, stringValue, strlen(stringValue));
        dst->mType = kTypeString;
    } else if (item.find(&refValue)) {
        if (refValue != NULL) { refValue->incStrong(this); }
        dst->u.refValue = refValue.get();
        dst->mType = kTypeObject;
    } else if (item.find(&msgValue)) {
        if (msgValue != NULL) { msgValue->incStrong(this); }
        dst->u.refValue = msgValue.get();
        dst->mType = kTypeMessage;
	} else {
        // unsupported item - we should not be here.
        dst->mType = kTypeInt32;
        dst->u.int32Value = 0xDEADDEAD;
        return BAD_TYPE;
    }
    return OK;
}

dp_state TSPMessage::removeEntryAt(size_t index) {
    if (index >= mNumItems) {
        return BAD_INDEX;
    }
    // delete entry data and objects
    --mNumItems;
    delete[] mItems[index].mName;
    mItems[index].mName = nullptr;
    freeItemValue(&mItems[index]);

    // swap entry with last entry and clear last entry's data
    if (index < mNumItems) {
        mItems[index] = mItems[mNumItems];
        mItems[mNumItems].mName = nullptr;
        mItems[mNumItems].mType = kTypeInt32;
    }
    return OK;
}

void TSPMessage::setItem(const char *name, const ItemData &item) {
    if (item.used()) {
        Item *it = allocateItem(name);
        if (it != nullptr) {
            setEntryAt(it - mItems, item);
        }
    }
}

TSPMessage::ItemData TSPMessage::findItem(const char *name) const {
    return getEntryAt(findEntryByName(name));
}

void TSPMessage::extend(const sp<TSPMessage> &other) {
    // ignore null messages
    if (other == nullptr) {
        return;
    }

    for (size_t ix = 0; ix < other->mNumItems; ++ix) {
        Item *it = allocateItem(other->mItems[ix].mName);
        if (it != nullptr) {
            ItemData data = other->getEntryAt(ix);
            setEntryAt(it - mItems, data);
        }
    }
}

size_t TSPMessage::findEntryByName(const char *name) const {
    return name == nullptr ? countEntries() : findItemIndex(name, strlen(name));
}

