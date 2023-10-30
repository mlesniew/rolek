<script setup>
import { ref, onMounted } from "vue";
import axios from "axios";
import RolekButton from "./RolekButton.vue";

const props = defineProps(["disabled"]);
const emit = defineEmits([
  "request_start",
  "request_success",
  "request_failure",
  "request_end",
]);

const names = ref(null);
const error = ref(false);

const reload = () => {
  error.value = false;
  names.value = null;
  axios
    .get("shutters")
    .then((response) => {
      names.value = response.data["shutters"].concat(response.data["groups"]);
      names.value.sort();
    })
    .catch(() => {
      error.value = true;
    });
};

onMounted(reload);
</script>

<template>
  <div class="row justify-content-left align-items-center">
    <template v-if="names">
      <RolekButton
        v-for="name in names"
        v-bind:key="name"
        :name="name"
        :disabled="props.disabled"
        @request_start="emit('request_start')"
        @request_success="emit('request_success')"
        @request_failure="emit('request_failure')"
        @request_end="emit('request_end')"
      />
    </template>

    <template v-else-if="error">
      <div class="col-3"></div>
      <div class="col-6 text-center" role="alert">
        <p>An error occurred.</p>
        <button class="btn btn-secondary btn-small" @click="reload">
          Retry
        </button>
      </div>
    </template>

    <template v-else>
      <div class="col-12 text-center">
        <div class="spinner-border" role="status"></div>
      </div>
    </template>
  </div>
</template>
