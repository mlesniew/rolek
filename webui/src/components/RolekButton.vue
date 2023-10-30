<script setup>
import { ref } from "vue";
import axios from "axios";

const props = defineProps(["name", "large", "disabled"]);
const emit = defineEmits([
  "request_start",
  "request_success",
  "request_failure",
  "request_end",
]);

const request = (direction) => {
  let url = "/shutters/";
  if (props.name) {
    url += props.name + "/";
  }

  url += direction;

  emit("request_start");
  axios
    .post(url)
    .then(() => {
      emit("request_success");
    })
    .catch(() => {
      emit("request_failure");
    })
    .then(() => {
      emit("request_end");
    });
};

const upDownButtonClass = ref("btn btn-primary");
const buttonGroupClass = ref("btn-group w-100 mb-sm-0 mb-3");

if (props.large) {
  buttonGroupClass.value += " btn-group-lg";
}

const stopButtonClass = ref(upDownButtonClass.value);
if (props.name) {
  stopButtonClass.value += " w-100";
}
</script>

<template>
  <div class="col-12 col-sm-6 col-md-4 p-1">
    <div :class="buttonGroupClass" role="group">
      <button
        type="button"
        :class="upDownButtonClass"
        @click="request('down')"
        :disabled="disabled"
      >
        &#9660;
      </button>
      <button
        type="button"
        :class="stopButtonClass"
        @click="request('stop')"
        :disabled="disabled"
      >
        {{ name || "&#8728;" }}
      </button>
      <button
        type="button"
        :class="upDownButtonClass"
        @click="request('up')"
        :disabled="disabled"
      >
        &#9650;
      </button>
    </div>
  </div>
</template>
