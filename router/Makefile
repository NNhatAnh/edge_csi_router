# Build daemon wifi_sensing với model do sensing/train.py xuất ra (--features inv|inv_nb --chains 3).
#   make MODEL=../models/<ts>          # host (kiểm parity) + aarch64 tĩnh cho router
#   make check MODEL=../models/<ts> RUN=../data/raw/<lượt có relay.bin>
# Cross-compile bằng zig trong .venv (pip install ziglang): binary tĩnh musl, không phụ thuộc thư viện router.
MODEL ?= $(error đặt MODEL=../models/<ts> (thư mục có csi_rf_model.c, feature_order.json))
ZIG   ?= ../.venv/bin/python -m ziglang cc
PY    ?= ../.venv/bin/python

all: build/wifi_sensing build/wifi_sensing_host

# feature_map.h: vị trí từng feature của model trong vector daemon [inv (18) | nb_inv (18)]
build/model.c: $(MODEL)/csi_rf_model.c $(MODEL)/feature_order.json
	mkdir -p build
	$(PY) -c "import json,sys; sys.path.insert(0,'../sensing'); import features as F; \
	  o=json.load(open('$(MODEL)/feature_order.json')); e=F.invariant_names(3)+F.baseline_names(3); \
	  bad=[c for c in o if c not in e]; assert not bad, f'model dùng feature daemon không có (train --features inv|inv_nb --chains 3): {bad}'; \
	  open('build/feature_map.h','w').write('static const int FMAP[] = {%s};\n#define NEEDS_BASELINE %d\n' % (','.join(str(e.index(c)) for c in o), any(c.startswith('nb_') for c in o)))"
	cp $(MODEL)/csi_rf_model.c $@
	cp $(MODEL)/report.json build/model_report.json

build/wifi_sensing: wifi_sensing.c build/model.c
	$(ZIG) -target aarch64-linux-musl -static -Os -Ibuild -o $@ wifi_sensing.c -lm

build/wifi_sensing_host: wifi_sensing.c build/model.c
	cc -O2 -Wall -Ibuild -o $@ wifi_sensing.c -lm

check: build/wifi_sensing_host
	$(PY) check_parity.py $(RUN)

clean:
	rm -rf build
